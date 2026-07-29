// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internals/webui/notebooks_internals_page_handler.h"

#include <memory>
#include <utility>

#include "base/observer_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/notebooks/internals/webui/notebooks_internals.mojom.h"
#include "components/notebooks/public/features.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace notebooks {
namespace {

constexpr char kTestNotebookHomeURL[] = "https://example.com";

class MockPage : public notebooks_internals::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<notebooks_internals::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnProfileEligibilityChanged(
      notebooks_internals::mojom::ProfileEligibilityPtr eligibility) override {
    eligibility_future_.SetValue(std::move(eligibility));
  }

  notebooks_internals::mojom::ProfileEligibilityPtr WaitForEligibilityChange() {
    return eligibility_future_.Take();
  }

 private:
  mojo::Receiver<notebooks_internals::mojom::Page> receiver_{this};
  base::test::TestFuture<notebooks_internals::mojom::ProfileEligibilityPtr>
      eligibility_future_;
};

class MockNotebooksEligibilityService : public NotebooksEligibilityService {
 public:
  MockNotebooksEligibilityService() = default;
  ~MockNotebooksEligibilityService() override = default;

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool IsEligible() const override { return is_eligible_; }

  bool IsEligibilityLoading() const override { return is_loading_; }

  void SetIsUserEligible(bool eligible) {
    is_eligible_ = eligible;
    for (auto& observer : observers_) {
      observer.OnNotebooksEligibilityChanged(eligible);
    }
  }

 private:
  bool is_eligible_ = false;
  bool is_loading_ = false;
  base::ObserverList<Observer> observers_;
};

class NotebooksInternalsPageHandlerTest : public testing::Test {
 public:
  NotebooksInternalsPageHandlerTest() = default;
  ~NotebooksInternalsPageHandlerTest() override = default;

  void SetUp() override {
    page_handler_ = std::make_unique<NotebooksInternalsPageHandler>(
        page_handler_remote_.BindNewPipeAndPassReceiver(),
        mock_page_.BindAndGetRemote(), nullptr,
        &notebooks_eligibility_service_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockNotebooksEligibilityService notebooks_eligibility_service_;
  MockPage mock_page_;
  mojo::Remote<notebooks_internals::mojom::PageHandler> page_handler_remote_;
  std::unique_ptr<NotebooksInternalsPageHandler> page_handler_;
};

TEST_F(NotebooksInternalsPageHandlerTest,
       GetFeatureFlagState_ReturnsCurrentFeatureState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kNotebooks,
      {{features::kNotebookHomeURL.name, kTestNotebookHomeURL}});

  base::test::TestFuture<notebooks_internals::mojom::FeatureFlagStatePtr>
      future;
  page_handler_remote_->GetFeatureFlagState(future.GetCallback());

  auto flags = future.Take();
  EXPECT_TRUE(flags->notebooks_feature_enabled);
  EXPECT_EQ(GURL(kTestNotebookHomeURL), flags->notebook_home_url);
}

TEST_F(NotebooksInternalsPageHandlerTest,
       GetProfileEligibility_ReturnsCurrentEligibility) {
  notebooks_eligibility_service_.SetIsUserEligible(true);

  base::test::TestFuture<notebooks_internals::mojom::ProfileEligibilityPtr>
      future;
  page_handler_remote_->GetProfileEligibility(future.GetCallback());

  auto eligibility = future.Take();
  EXPECT_TRUE(eligibility->user_eligible);
}

TEST_F(NotebooksInternalsPageHandlerTest,
       OnNotebooksEligibilityChanged_NotifiesPage) {
  notebooks_eligibility_service_.SetIsUserEligible(true);

  auto eligibility = mock_page_.WaitForEligibilityChange();
  ASSERT_TRUE(eligibility);
  EXPECT_TRUE(eligibility->user_eligible);
}

}  // namespace
}  // namespace notebooks
