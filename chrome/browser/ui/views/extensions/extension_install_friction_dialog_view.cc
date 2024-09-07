// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_friction_dialog_view.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

void AutoConfirmDialog(base::OnceCallback<void(bool)> callback) {
  switch (extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue()) {
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), true));
      return;
    case extensions::ScopedTestDialogAutoConfirm::CANCEL:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false));
      return;
    default:
      NOTREACHED();
  }
}

}  // namespace

namespace extensions {

void ShowExtensionInstallFrictionDialog(
    content::WebContents* contents,
    base::OnceCallback<void(bool)> callback) {
  if (extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue() !=
      extensions::ScopedTestDialogAutoConfirm::NONE) {
    AutoConfirmDialog(std::move(callback));
    return;
  }

  auto* view =
      new ExtensionInstallFrictionDialogView(contents, std::move(callback));

  constrained_window::CreateBrowserModalDialogViews(
      view, contents->GetTopLevelNativeWindow())
      ->Show();
}

}  // namespace extensions

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExtensionInstallFrictionDialogAction {
  kClose = 0,
  kLearnMore = 1,
  kContinueToInstall = 2,
  kMaxValue = kContinueToInstall,
};

void ReportExtensionInstallFrictionDialogAction(
    ExtensionInstallFrictionDialogAction action) {
  base::UmaHistogramEnumeration("Extensions.InstallFrictionDialogAction",
                                action);
}

}  // namespace

ExtensionInstallFrictionDialogView::ExtensionInstallFrictionDialogView(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      parent_web_contents_(web_contents->GetWeakPtr()),
      callback_(std::move(callback)) {
  SetModalType(ui::mojom::ModalType::kWindow);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_EXTENSION_PROMPT_INSTALL_FRICTION_CONTINUE_BUTTON));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_CLOSE));

  SetShowIcon(true);
  SetTitle(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_INSTALL_FRICTION_TITLE));

  SetAcceptCallback(base::BindOnce(
      [](ExtensionInstallFrictionDialogView* view) { view->accepted_ = true; },
      base::Unretained(this)));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
  set_draggable(true);

  auto warning_label = CreateWarningLabel();
  auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetContents(std::move(warning_label));
  scroll_view->ClipHeightTo(
      0, provider->GetDistanceMetric(
             views::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));
}

std::unique_ptr<views::StyledLabel>
ExtensionInstallFrictionDialogView::CreateWarningLabel() {
  auto label = std::make_unique<views::StyledLabel>();

  std::u16string warning_text = l10n_util::GetStringUTF16(
      IDS_EXTENSION_PROMPT_INSTALL_FRICTION_WARNING_TEXT);
  std::u16string learn_more_text = l10n_util::GetStringUTF16(IDS_LEARN_MORE);

  std::u16string text = base::StrCat({warning_text, u" ", learn_more_text});

  label->SetText(text);
  gfx::Range details_range(warning_text.length() + 1, text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &ExtensionInstallFrictionDialogView::OnLearnMoreLinkClicked,
          base::Unretained(this)));

  label->AddStyleRange(details_range, link_style);
  return label;
}

ExtensionInstallFrictionDialogView::~ExtensionInstallFrictionDialogView() {
  // Another modal dialog (ExtensionInstallDialogView) is displayed when
  // clicking through this friction dialog. The callback is invoked in the
  // destructor to make sure the current modal dialog is closed before showing
  // the next one.
  //
  // On MacOS, a modal dialog silently fails to display if another modal dialog
  // is already displayed. The issue is tracked in crbug.com/1199383
  ExtensionInstallFrictionDialogAction action;
  if (accepted_) {
    action = ExtensionInstallFrictionDialogAction::kContinueToInstall;
  } else if (learn_more_clicked_) {
    action = ExtensionInstallFrictionDialogAction::kLearnMore;
  } else {
    action = ExtensionInstallFrictionDialogAction::kClose;
  }
  ReportExtensionInstallFrictionDialogAction(action);

  std::move(callback_).Run(accepted_);
}

// override
ui::ImageModel ExtensionInstallFrictionDialogView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kGppMaybeIcon, ui::kColorAlertMediumSeverityIcon,
      extension_misc::EXTENSION_ICON_SMALLISH);
}

void ExtensionInstallFrictionDialogView::OnLearnMoreLinkClicked() {
  GURL url(chrome::kCwsEnhancedSafeBrowsingLearnMoreURL);
  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);

  learn_more_clicked_ = true;
  if (parent_web_contents_) {
    parent_web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
  } else {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
    displayer.browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  }

  CancelDialog();

  // TODO(jeffcyr): Record UMA metric
}

void ExtensionInstallFrictionDialogView::ClickLearnMoreLinkForTesting() {
  OnLearnMoreLinkClicked();
}

BEGIN_METADATA(ExtensionInstallFrictionDialogView)
END_METADATA
