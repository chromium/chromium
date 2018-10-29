// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
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

// A custom view for the title label that will be ignored by screen readers
// (since the PermissionsBubble handles the context).
class PermissionsLabel : public views::Label {
 public:
  explicit PermissionsLabel(const base::string16& text)
      : views::Label(text, views::style::CONTEXT_DIALOG_TITLE) {}
  ~PermissionsLabel() override {}

  // views::Label:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kIgnored;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionsLabel);
};

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
  int GetDefaultDialogButton() const override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
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
  DCHECK(!requests.empty());

  set_close_on_deactivate(false);

#if defined(OS_MACOSX)
  // On Mac, the browser UI flips depending on a runtime feature. TODO(tapted):
  // Change the default in views::PlatformStyle when features::kMacRTL launches,
  // and remove the following.
  if (base::FeatureList::IsEnabled(features::kMacRTL))
    set_mirror_arrow_in_rtl(true);
#endif

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  for (size_t index = 0; index < requests.size(); index++) {
    views::View* label_container = new views::View();
    int indent =
        provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
    label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kHorizontal, gfx::Insets(0, indent),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
    views::ImageView* icon = new views::ImageView();
    const gfx::VectorIcon& vector_id = requests[index]->GetIconId();
    icon->SetImage(gfx::CreateVectorIcon(vector_id, kPermissionIconSize,
                                         gfx::kChromeIconGrey));
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

  std::unique_ptr<views::Label> title =
      std::make_unique<PermissionsLabel>(GetWindowTitle());
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->set_collapse_when_hidden(true);
  title->SetMultiLine(true);

  // Elide from head in order to keep the most significant part of the origin
  // and avoid spoofing. Note that in English, GetWindowTitle() returns a string
  // "$ORIGIN wants to", so the "wants to" will not be elided. In other
  // languages, the non-origin part may appear fully or partly before the origin
  // (e.g., in Filipino, "Gusto ng $ORIGIN na"), which means it may be elided.
  // This is not optimal, but it is necessary to avoid origin spoofing. See
  // crbug.com/774438.
  title->SetElideBehavior(gfx::ELIDE_HEAD);

  // Multiline breaks elision, which would mean a very long origin gets
  // truncated from the least significant side. Explicitly disable multiline.
  title->SetMultiLine(false);
  GetBubbleFrameView()->SetTitleView(std::move(title));
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

int PermissionsBubbleDialogDelegateView::GetDefaultDialogButton() const {
  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  return ui::DIALOG_BUTTON_NONE;
}

int PermissionsBubbleDialogDelegateView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

base::string16 PermissionsBubbleDialogDelegateView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);

  // The text differs based on whether OK is the only visible button.
  return l10n_util::GetStringUTF16(GetDialogButtons() == ui::DIALOG_BUTTON_OK
                                       ? IDS_OK
                                       : IDS_PERMISSION_ALLOW);
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
    : browser_(browser), delegate_(delegate), bubble_delegate_(nullptr) {
  Show();
}

PermissionPromptImpl::~PermissionPromptImpl() {
  if (bubble_delegate_)
    bubble_delegate_->CloseBubble();
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
