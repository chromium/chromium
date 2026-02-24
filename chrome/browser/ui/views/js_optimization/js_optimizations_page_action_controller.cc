// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/js_optimization/js_optimizations_page_action_controller.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/site_protection/site_familiarity_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/js_optimization/js_optimizations_infobar_delegate.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(JsOptimizationsPageActionController,
                                      kBubbleBodyElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(JsOptimizationsPageActionController,
                                      kBubbleButtonElementId);

namespace {
JsOptimizationsPageActionController::BubbleCreatedCallback& GetTestCallback() {
  static base::NoDestructor<
      JsOptimizationsPageActionController::BubbleCreatedCallback>
      callback;
  return *callback;
}
}  // namespace

// static
void JsOptimizationsPageActionController::SetBubbleCreatedCallbackForTesting(
    BubbleCreatedCallback callback) {
  GetTestCallback() = std::move(callback);
}

JsOptimizationsPageActionController::JsOptimizationsPageActionController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : tabs::ContentsObservingTabFeature(tab_interface),
      page_action_controller_(page_action_controller) {
  UpdateIconVisibility();
}

JsOptimizationsPageActionController::~JsOptimizationsPageActionController() =
    default;

void JsOptimizationsPageActionController::PrimaryPageChanged(
    content::Page& page) {
  UpdateIconVisibility();
}

void JsOptimizationsPageActionController::UpdateIconVisibility() {
  if (site_protection::AreV8OptimizationsDisabled(web_contents()) == true) {
    page_action_controller_->Show(kActionShowJsOptimizationsIcon);
  } else {
    page_action_controller_->Hide(kActionShowJsOptimizationsIcon);
  }
}

void JsOptimizationsPageActionController::ShowBubble(
    views::BubbleAnchor anchor,
    actions::ActionItem* action_item) {
  bubble_ = CreateBubble(anchor, action_item);
  action_item->SetIsShowingBubble(true);
}

void JsOptimizationsPageActionController::OnBubbleHidden(
    actions::ActionItem* action_item) {
  action_item->SetIsShowingBubble(false);
}

void JsOptimizationsPageActionController::OnWidgetClosing(
    views::Widget* widget) {
  widget_observation_.Reset();
  bubble_ = nullptr;
}

views::BubbleDialogModelHost* JsOptimizationsPageActionController::CreateBubble(
    views::BubbleAnchor anchor,
    actions::ActionItem* action_item) {
  auto dialog_model_builder = ui::DialogModel::Builder();
  dialog_model_builder
      .SetTitle(l10n_util::GetStringUTF16(IDS_JS_OPTIMIZATION_BUBBLE_TITLE))
      .SetDialogDestroyingCallback(base::BindOnce(
          &JsOptimizationsPageActionController::OnBubbleHidden,
          weak_factory_.GetWeakPtr(), base::Unretained(action_item)));
  // When v8 optimizations are disabled by an enterprise policy, we don't give
  // the user the option to change it. Otherwise, we do.
  if (site_protection::GetJavascriptOptimizerSettingSource(web_contents()) ==
      content_settings::SettingSource::kPolicy) {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel(
            l10n_util::GetStringUTF16(IDS_JS_OPTIMIZATION_BUBBLE_POLICY_BODY)),
        std::u16string(),
        JsOptimizationsPageActionController::kBubbleBodyElementId);
  } else {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel(l10n_util::GetStringUTF16(
            IDS_JS_OPTIMIZATION_BUBBLE_BODY_NON_POLICY_BODY)),
        std::u16string(),
        JsOptimizationsPageActionController::kBubbleBodyElementId);

    dialog_model_builder.AddOkButton(
        base::BindOnce(
            &JsOptimizationsPageActionController::EnableV8Optimizations,
            weak_factory_.GetWeakPtr()),
        ui::DialogModel::Button::Params()
            .SetLabel(l10n_util::GetStringUTF16(
                IDS_JS_OPTIMIZATION_BUBBLE_ENABLE_BUTTON))
            .SetId(
                JsOptimizationsPageActionController::kBubbleButtonElementId));
  }

  auto dialog_model = dialog_model_builder.Build();
  auto bubble_unique = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor, views::BubbleBorder::TOP_RIGHT);
  auto* bubble = bubble_unique.get();
  if (GetTestCallback()) {
    GetTestCallback().Run(bubble);
  }
  // TODO(crbug.com/464011395): Refactor to use CLIENT_OWNS_WIDGET.
  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_unique));
  widget_observation_.Observe(widget);
  widget->Show();
  return bubble;
}

void JsOptimizationsPageActionController::EnableV8Optimizations() {
  site_protection::EnableV8Optimizations(web_contents());

  // TODO(crbug.com/457420369): Something may need to be done here to cause
  // the updated content setting to take effect in the existing tab. Currently
  // it only takes effect in subsequently opened tabs.

  // Display a prompt to the user to reload the page
  infobars::ContentInfoBarManager* const infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  if (infobar_manager) {
    JsOptimizationsInfoBarDelegate::Create(infobar_manager);
  }
}
