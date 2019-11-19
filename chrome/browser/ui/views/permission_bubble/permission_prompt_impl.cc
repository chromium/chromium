// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/front_eliding_title_label.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

#if defined(OS_MACOSX)
#include "chrome/common/chrome_features.h"
#endif

using bubble_anchor_util::AnchorConfiguration;

namespace {

// (Square) pixel size of icon.
constexpr int kPermissionIconSize = 18;

// Returns the view to anchor the permission bubble to (may be null) and the
// arrow position of the bubble.
AnchorConfiguration GetPermissionAnchorConfiguration(Browser* browser) {
  return bubble_anchor_util::GetPageInfoAnchorConfiguration(browser);
}

// Returns the anchor rect to anchor the permission bubble to, as a fallback.
// Only used if GetPermissionAnchorView() returns nullptr.
gfx::Rect GetPermissionAnchorRect(Browser* browser) {
  return bubble_anchor_util::GetPageInfoAnchorRect(browser);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// View implementation for the permissions bubble.
class PermissionsBubbleDialogDelegateView
    : public views::BubbleDialogDelegateView {
 public:
  PermissionsBubbleDialogDelegateView(
      PermissionPromptImpl* owner,
      const std::vector<PermissionRequest*>& requests,
      const PermissionPrompt::DisplayNameOrOrigin& name_or_origin);
  ~PermissionsBubbleDialogDelegateView() override;

  void CloseBubble();

  // BubbleDialogDelegateView overrides.
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  void AddedToWidget() override;
  void SizeToContents() override;

  // Repositions the bubble so it's displayed in the correct location based on
  // the updated anchor view, or anchor rect if that is (or became) null.
  void UpdateAnchor();

 private:
  PermissionPromptImpl* owner_;
  PermissionPrompt::DisplayNameOrOrigin name_or_origin_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsBubbleDialogDelegateView);
};

PermissionsBubbleDialogDelegateView::PermissionsBubbleDialogDelegateView(
    PermissionPromptImpl* owner,
    const std::vector<PermissionRequest*>& requests,
    const PermissionPrompt::DisplayNameOrOrigin& name_or_origin)
    : owner_(owner), name_or_origin_(name_or_origin) {
  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL, l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));

  DCHECK(!requests.empty());

  set_close_on_deactivate(false);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  for (size_t index = 0; index < requests.size(); index++) {
    views::View* label_container = new views::View();
    int indent =
        provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
    label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(0, indent),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
    views::ImageView* icon = new views::ImageView();
    const gfx::VectorIcon& vector_id = requests[index]->GetIconId();
    const SkColor icon_color = icon->GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_DefaultIconColor);
    icon->SetImage(
        gfx::CreateVectorIcon(vector_id, kPermissionIconSize, icon_color));
    label_container->AddChildView(icon);
    views::Label* label =
        new views::Label(requests.at(index)->GetMessageTextFragment());
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_container->AddChildView(label);
    AddChildView(label_container);
  }

  chrome::RecordDialogCreation(chrome::DialogIdentifier::PERMISSIONS);
}

PermissionsBubbleDialogDelegateView::~PermissionsBubbleDialogDelegateView() {
  if (owner_)
    owner_->Closing();
}

void PermissionsBubbleDialogDelegateView::CloseBubble() {
  owner_ = nullptr;
  GetWidget()->Close();
}

void PermissionsBubbleDialogDelegateView::AddedToWidget() {
  // There is no URL spoofing risk from non-origins.
  if (!name_or_origin_.is_origin)
    return;

  GetBubbleFrameView()->SetTitleView(
      CreateFrontElidingTitleLabel(GetWindowTitle()));
}

bool PermissionsBubbleDialogDelegateView::ShouldShowCloseButton() const {
  return true;
}

base::string16 PermissionsBubbleDialogDelegateView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    name_or_origin_.name_or_origin);
}

void PermissionsBubbleDialogDelegateView::SizeToContents() {
  BubbleDialogDelegateView::SizeToContents();
}

void PermissionsBubbleDialogDelegateView::OnWidgetDestroying(
    views::Widget* widget) {
  views::BubbleDialogDelegateView::OnWidgetDestroying(widget);
  if (owner_) {
    owner_->Closing();
    owner_ = nullptr;
  }
}

bool PermissionsBubbleDialogDelegateView::Cancel() {
  if (owner_)
    owner_->Deny();
  return true;
}

bool PermissionsBubbleDialogDelegateView::Accept() {
  if (owner_)
    owner_->Accept();
  return true;
}

bool PermissionsBubbleDialogDelegateView::Close() {
  // Neither explicit accept nor explicit deny.
  return true;
}

void PermissionsBubbleDialogDelegateView::UpdateAnchor() {
  AnchorConfiguration configuration =
      GetPermissionAnchorConfiguration(owner_->browser());
  SetAnchorView(configuration.anchor_view);
  SetHighlightedButton(configuration.highlighted_button);
  if (!configuration.anchor_view)
    SetAnchorRect(GetPermissionAnchorRect(owner_->browser()));
  SetArrow(configuration.bubble_arrow);
}

//////////////////////////////////////////////////////////////////////////////
// PermissionPromptImpl

PermissionPromptImpl::PermissionPromptImpl(Browser* browser, Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      bubble_delegate_(nullptr),
      web_contents_(browser->tab_strip_model()->GetActiveWebContents()) {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents_);
  if (manager->ShouldShowQuietPermissionPrompt()) {
    show_quiet_permission_prompt_ = true;
    // Show the prompt as an indicator in the right side of the omnibox.
    content_settings::UpdateLocationBarUiForWebContents(web_contents_);
  } else {
    Show();
  }
}

PermissionPromptImpl::~PermissionPromptImpl() {
  if (bubble_delegate_)
    bubble_delegate_->CloseBubble();

  if (show_quiet_permission_prompt_) {
    // Update location bar to hide the permission prompt if it is shown as a
    // quiet permission prompt.
    content_settings::UpdateLocationBarUiForWebContents(web_contents_);
  }
}

void PermissionPromptImpl::UpdateAnchorPosition() {
  DCHECK(browser_);
  DCHECK(browser_->window());

  if (bubble_delegate_) {
    bubble_delegate_->set_parent_window(
        platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));
    bubble_delegate_->UpdateAnchor();
  }
}

gfx::NativeWindow PermissionPromptImpl::GetNativeWindow() {
  if (bubble_delegate_ && bubble_delegate_->GetWidget())
    return bubble_delegate_->GetWidget()->GetNativeWindow();
  return nullptr;
}

PermissionPrompt::TabSwitchingBehavior
PermissionPromptImpl::GetTabSwitchingBehavior() {
  return PermissionPrompt::TabSwitchingBehavior::
      kDestroyPromptButKeepRequestPending;
}

void PermissionPromptImpl::Closing() {
  if (bubble_delegate_)
    bubble_delegate_ = nullptr;
  if (delegate_)
    delegate_->Closing();
}

void PermissionPromptImpl::Accept() {
  if (delegate_)
    delegate_->Accept();
}

void PermissionPromptImpl::Deny() {
  if (delegate_)
    delegate_->Deny();
}

void PermissionPromptImpl::Show() {
  DCHECK(browser_);
  DCHECK(browser_->window());

  bubble_delegate_ = new PermissionsBubbleDialogDelegateView(
      this, delegate_->Requests(), delegate_->GetDisplayNameOrOrigin());

  // Set |parent_window| because some valid anchors can become hidden.
  bubble_delegate_->set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble_delegate_);
  // If a browser window (or popup) other than the bubble parent has focus,
  // don't take focus.
  if (browser_->window()->IsActive())
    widget->Show();
  else
    widget->ShowInactive();

  bubble_delegate_->SizeToContents();

  bubble_delegate_->UpdateAnchor();
}
