// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"

#include <optional>

#include "base/callback_list.h"
#include "base/check.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"

// Forwarding precondition that releases its reference when the context is
// invalidated or destroyed.
class BrowserUserEducationContext::ForwardingPrecondition
    : public user_education::ForwardingFeaturePromoPrecondition {
 public:
  ForwardingPrecondition(const FeaturePromoPrecondition& source,
                         BrowserUserEducationContext& context)
      : ForwardingFeaturePromoPrecondition(source),
        invalidate_subscription_(context.invalidate_callbacks_.Add(
            base::BindOnce(&ForwardingPrecondition::Invalidate,
                           base::Unretained(this)))) {}

 private:
  base::CallbackListSubscription invalidate_subscription_;
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(BrowserUserEducationContext)

BrowserUserEducationContext::BrowserUserEducationContext(
    BrowserView& browser_view,
    const user_education::UserEducationTimeProvider& time_provider)
    : browser_view_(&browser_view) {
  CreateSharedPreconditions(time_provider);
}

BrowserUserEducationContext::~BrowserUserEducationContext() {
  invalidate_callbacks_.Notify();
}

bool BrowserUserEducationContext::IsValid() const {
  return browser_view_.get();
}

ui::ElementContext BrowserUserEducationContext::GetElementContext() const {
  CHECK(IsValid());
  return browser_view_->GetElementContext();
}

const ui::AcceleratorProvider*
BrowserUserEducationContext::GetAcceleratorProvider() const {
  CHECK(IsValid());
  return browser_view_.get();
}

BrowserUserEducationContext::PreconditionPtr
BrowserUserEducationContext::GetSharedPrecondition(PreconditionId id) {
  const auto it = shared_preconditions_.find(id);
  CHECK(it != shared_preconditions_.end());
  return std::make_unique<ForwardingPrecondition>(*it->second, *this);
}

void BrowserUserEducationContext::Invalidate(
    base::PassKey<BrowserUserEducationInterfaceImpl>) {
  invalidate_callbacks_.Notify();
  shared_preconditions_.clear();
  browser_view_ = nullptr;
}

BrowserView& BrowserUserEducationContext::GetBrowserView() const {
  CHECK(IsValid());
  return *browser_view_;
}

void BrowserUserEducationContext::CreateSharedPreconditions(
    const user_education::UserEducationTimeProvider& time_provider) {
  // Shared preconditions only apply in User Education 2.5.
  if (!user_education::features::IsUserEducationV25()) {
    return;
  }
  CHECK(shared_preconditions_.empty());

  // Hold off showing most promos while the omnibox is open.
  PreconditionPtr ptr =
      std::make_unique<OmniboxNotOpenPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Hold off most promos when the content is in fullscreen.
  ptr = std::make_unique<ContentNotFullscreenPrecondition>(
      *browser_view_->browser());
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Hold off most promos when the toolbar is collapsed.
  ptr = std::make_unique<ToolbarNotCollapsedPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Do not show promos when the browser is closing.
  ptr = std::make_unique<BrowserNotClosingPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Do not show promos when a critical notice is showing.
  ptr = std::make_unique<NoCriticalNoticeShowingPrecondition>(*browser_view_);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);

  // Ensure that this uses the same time source as the rest of the User
  // Education system, so tests are consistent.
  ptr = std::make_unique<UserNotActivePrecondition>(*browser_view_,
                                                    time_provider);
  CHECK(shared_preconditions_.emplace(ptr->GetIdentifier(), std::move(ptr))
            .second);
}
