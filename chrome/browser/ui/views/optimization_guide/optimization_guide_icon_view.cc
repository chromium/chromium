// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/optimization_guide/optimization_guide_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/icon_view_metadata.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

OptimizationGuideIconView::OptimizationGuideIconView(
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    Browser* browser)
    : PageActionIconView(nullptr,
                         0,
                         parent_delegate,
                         delegate,
                         "OptimizationGuide"),
      optimization_guide_service_(
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              browser->profile())) {
  SetProperty(views::kElementIdentifierKey, kOptimizationGuideChipElementId);
  GetViewAccessibility().SetName(u"OptimizationGuide");
  if (optimization_guide::features::ShouldEnableOptimizationGuideIconView() &&
      optimization_guide_service_) {
    optimization_guide_service_->RegisterOptimizationTypes(
        {optimization_guide::proto::OPTIMIZATION_GUIDE_ICON_VIEW});
  }
}

OptimizationGuideIconView::~OptimizationGuideIconView() = default;

views::BubbleDialogDelegate* OptimizationGuideIconView::GetBubble() const {
  return nullptr;
}

void OptimizationGuideIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  // Don't do anything.
}

const gfx::VectorIcon& OptimizationGuideIconView::GetVectorIcon() const {
  return vector_icons::kGlobeIcon;
}

void OptimizationGuideIconView::UpdateImpl() {
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);

  std::optional<optimization_guide::proto::OptimizationGuideIconViewMetadata>
      metadata = GetMetadata();
  SetVisible(metadata.has_value());
  SetLabel(metadata.has_value() ? base::UTF8ToUTF16(metadata->cue_label())
                                : u"");
}

std::optional<optimization_guide::proto::OptimizationGuideIconViewMetadata>
OptimizationGuideIconView::GetMetadata() const {
  if (!optimization_guide_service_) {
    return std::nullopt;
  }
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return std::nullopt;
  }

  optimization_guide::OptimizationMetadata metadata;
  auto decision = optimization_guide_service_->CanApplyOptimization(
      web_contents->GetLastCommittedURL(),
      optimization_guide::proto::OPTIMIZATION_GUIDE_ICON_VIEW, &metadata);
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return std::nullopt;
  }
  return metadata.ParsedMetadata<
      optimization_guide::proto::OptimizationGuideIconViewMetadata>();
}

BEGIN_METADATA(OptimizationGuideIconView)
END_METADATA
