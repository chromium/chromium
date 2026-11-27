// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_autofill_driver_factory.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace autofill {

namespace {

template <typename T>
  requires(std::derived_from<TestAutofillDriver, std::remove_const_t<T>>)
T* ToPointer(const std::unique_ptr<TestAutofillDriver>& driver) {
  return driver.get();
}

}  // namespace

TestAutofillDriverFactory::ScopedValidationSuppressor::
    ScopedValidationSuppressor(TestAutofillDriverFactory& factory)
    : factory_(factory.weak_ptr_factory_.GetWeakPtr()) {
  ++factory_->num_validation_suppressors_;
}

TestAutofillDriverFactory::ScopedValidationSuppressor::
    ~ScopedValidationSuppressor() {
  if (factory_ && --factory_->num_validation_suppressors_ == 0 &&
      !factory_->drivers_.empty()) {
    factory_->CheckValidFrameTree();
  }
}

TestAutofillDriverFactory::TestAutofillDriverFactory(
    TestAutofillClientBase* client,
    base::PassKey<TestAutofillClientBase>)
    : client_(CHECK_DEREF(client)) {}

TestAutofillDriverFactory::~TestAutofillDriverFactory() {
  for (auto& observer : AutofillDriverFactory::observers()) {
    observer.OnAutofillDriverFactoryDestroyed(*this);
  }
  // Destroy drivers in LIFO order because child TestAutofillDrivers hold a
  // raw_ptr to their parent driver and are appended to `drivers_` after their
  // parent.
  while (!drivers_.empty()) {
    drivers_.pop_back();
  }
}

bool TestAutofillDriverFactory::Owns(const TestAutofillDriver& driver) const {
  return std::ranges::contains(drivers_, &driver,
                               &ToPointer<const TestAutofillDriver>);
}

TestAutofillClient& TestAutofillDriverFactory::client() {
  // Only TestAutofillClient can construct a TestAutofillClientBase.
  return static_cast<TestAutofillClient&>(*client_);
}

void TestAutofillDriverFactory::Adopt(
    std::unique_ptr<TestAutofillDriver> driver) {
  TestAutofillDriver& raw_driver = *driver;
  drivers_.push_back(std::move(driver));
  for (auto& observer : observers()) {
    observer.OnAutofillDriverCreated(*this, raw_driver);
  }
}

TestAutofillDriver& TestAutofillDriverFactory::Create() {
  return Create<TestAutofillDriver, TestBrowserAutofillManager>();
}

TestAutofillDriver& TestAutofillDriverFactory::CreateAndActivate() {
  return CreateAndActivate<TestAutofillDriver, TestBrowserAutofillManager>();
}

void TestAutofillDriverFactory::Activate(TestAutofillDriver& driver) {
  CHECK(Owns(driver));
  driver.SetIsActive(true, /*pass_key=*/{});
  SetLifecycleStateAndNotifyObservers(driver, LifecycleState::kActive);
  CheckValidFrameTree();
}

void TestAutofillDriverFactory::Deactivate(TestAutofillDriver& driver) {
  CHECK(Owns(driver));
  driver.SetIsActive(false, /*pass_key=*/{});
  SetLifecycleStateAndNotifyObservers(driver, LifecycleState::kInactive);
  CheckValidFrameTree();
}

void TestAutofillDriverFactory::Reset(TestAutofillDriver& driver) {
  CHECK(Owns(driver));
  ScopedValidationSuppressor suppressor(*this);
  LifecycleState original_state = driver.GetLifecycleState();
  SetLifecycleStateAndNotifyObservers(driver, LifecycleState::kPendingReset);
  SetLifecycleStateAndNotifyObservers(driver, original_state);
}

void TestAutofillDriverFactory::Delete(TestAutofillDriver& driver) {
  CHECK(Owns(driver));
  const bool has_child = std::ranges::contains(
      drivers_, &driver, [](const std::unique_ptr<TestAutofillDriver>& d) {
        return d->GetParent();
      });
  CHECK(!has_child)
      << "Deleting a TestAutofillDriver while another owned driver still "
         "references it as its parent. Delete child drivers before their "
         "parent or call `SetParent(nullptr)` first.";
  ScopedValidationSuppressor suppressor(*this);
  SetLifecycleStateAndNotifyObservers(driver, LifecycleState::kPendingDeletion);
  auto it =
      std::ranges::find(drivers_, &driver, &ToPointer<TestAutofillDriver>);
  drivers_.erase(it);
}

void TestAutofillDriverFactory::DeleteAll() {
  ScopedValidationSuppressor suppressor(*this);
  for (const std::unique_ptr<TestAutofillDriver>& driver :
       std::views::reverse(drivers_)) {
    SetLifecycleStateAndNotifyObservers(*driver,
                                        LifecycleState::kPendingDeletion);
  }
  // Destroy drivers in LIFO order because child TestAutofillDrivers hold a
  // raw_ptr to their parent driver and are appended to `drivers_` after their
  // parent.
  while (!drivers_.empty()) {
    drivers_.pop_back();
  }
}

void TestAutofillDriverFactory::RequirePrimaryMainFrame(
    bool require_primary_main_frame) {
  require_primary_main_frame_ = require_primary_main_frame;
  if (!drivers_.empty()) {
    CheckValidFrameTree();
  }
}

std::vector<AutofillDriver*> TestAutofillDriverFactory::GetExistingDrivers() {
  if (!drivers_.empty()) {
    // Skip validation when no drivers exist yet: observers (e.g.,
    // ScopedAutofillManagersObservation) legitimately call GetExistingDrivers()
    // during client setup before any driver has been created.
    CheckValidFrameTree();
  }
  return base::ToVector(drivers_, &ToPointer<AutofillDriver>);
}

TestAutofillDriver* TestAutofillDriverFactory::driver(size_t index) {
  return index < drivers_.size() ? drivers_[index].get() : nullptr;
}

void TestAutofillDriverFactory::CheckValidFrameTree() const {
  constexpr char kSuppressMessage[] =
      "\n\nIf this CHECK is being hit while you're setting up the frame tree "
      "or is otherwise unintended, you can suppress it with "
      "`TestAutofillDriverFactory::ScopedValidationSuppressor`.";

  if (is_validation_suppressed()) {
    return;
  }

  base::flat_set<LocalFrameToken> seen_frame_tokens;
  for (const std::unique_ptr<TestAutofillDriver>& d : drivers_) {
    CHECK(!d->GetFrameToken().is_empty())
        << "A TestAutofillDriver must have a non-empty LocalFrameToken."
        << kSuppressMessage;
    CHECK(seen_frame_tokens.insert(d->GetFrameToken()).second)
        << "Duplicate LocalFrameToken " << d->GetFrameToken()
        << " across TestAutofillDrivers in the same factory."
        << kSuppressMessage;
  }

  for (const std::unique_ptr<TestAutofillDriver>& d : drivers_) {
    using enum AutofillDriver::LifecycleState;
    CHECK(d->GetLifecycleState() != kActive || d->IsActive())
        << "A TestAutofillDriver in LifecycleState::kActive must satisfy "
           "`IsActive()`."
        << kSuppressMessage;
    CHECK(d->GetLifecycleState() != kInactive || !d->IsActive())
        << "A TestAutofillDriver in LifecycleState::kInactive must not "
           "satisfy `IsActive()`."
        << kSuppressMessage;
  }

  for (const std::unique_ptr<TestAutofillDriver>& d : drivers_) {
    if (TestAutofillDriver* parent = d->GetParent()) {
      absl::flat_hash_set<const TestAutofillDriver*> visited = {d.get()};
      for (TestAutofillDriver* ancestor = parent; ancestor != nullptr;
           ancestor = ancestor->GetParent()) {
        CHECK(Owns(*ancestor))
            << "TestAutofillDriver has a parent that is not owned by this "
               "TestAutofillDriverFactory."
            << kSuppressMessage;
        CHECK(visited.insert(ancestor).second)
            << "Cycle detected in `TestAutofillDriver::GetParent()` chain."
            << kSuppressMessage;
      }
    }
  }

  for (const std::unique_ptr<TestAutofillDriver>& d : drivers_) {
    if (TestAutofillDriver* parent = d->GetParent()) {
      using enum AutofillDriver::LifecycleState;
      CHECK(!d->IsActive() || parent->IsActive())
          << "If the child TestAutofillDriver is active, then the parent must "
             "be active."
          << kSuppressMessage;
      CHECK(d->GetLifecycleState() != kActive ||
            parent->GetLifecycleState() == kActive)
          << "If the child TestAutofillDriver is active, then the parent must "
             "be active."
          << kSuppressMessage;
    }
  }

  for (const std::unique_ptr<TestAutofillDriver>& d : drivers_) {
    if (TestAutofillDriver* parent = d->GetParent()) {
      CHECK_EQ(d->IsEmbedded(), parent->IsEmbedded())
          << "Child TestAutofillDriver and its parent must agree on "
             "`IsEmbedded()`."
          << kSuppressMessage;
    }
  }

  const size_t num_primary_main_frames = std::ranges::count_if(
      drivers_, [](const std::unique_ptr<TestAutofillDriver>& d) {
        return d->IsActive() &&
               d->GetLifecycleState() ==
                   AutofillDriver::LifecycleState::kActive &&
               !d->IsEmbedded() && !d->GetParent();
      });
  CHECK(!require_primary_main_frame_ || num_primary_main_frames > 0)
      << "No active primary main frame AutofillDriver found.\n\n"
         "If your test is using `WithTestAutofillClientDriverManager`, "
         "call `CreateAndActivateAutofillDriver()` or "
         "`ActivateAutofillDriver()`.\n\n"
         "Otherwise, make sure it satisfies `IsActive()` and "
         "`GetLifecycleState() == kActive` and `!IsEmbedded()` and "
         "`GetParent() == nullptr`."
      << kSuppressMessage;
  CHECK_LE(num_primary_main_frames, 1u)
      << "Found " << num_primary_main_frames
      << " active primary main frame AutofillDrivers.\n\n"
         "If your test is using `WithTestAutofillClientDriverManager`, "
         "make sure at most one driver is activated without a parent or "
         "`IsEmbedded()` set (via `CreateAndActivateAutofillDriver()` or "
         "`ActivateAutofillDriver()`)."
      << kSuppressMessage;
}

}  // namespace autofill
