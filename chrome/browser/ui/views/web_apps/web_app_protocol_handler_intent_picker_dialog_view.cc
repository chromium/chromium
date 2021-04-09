// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_protocol_handler_intent_picker_dialog_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_hover_button.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace {

// Maximum numbers of web apps we want to show at a time in the dialog.
// The height of the scroll in the dialog depends on how many app
// candidates we got and how many we want to show. If there is more than
// |KMaxAppResults| app candidates, we will show 3.5 apps to let the user
// know there are more than |kMaxAppResults| apps accessible by scrolling
// the list.
constexpr size_t kMaxAppResults = 3;
// This dialog follows the design that
// chrome/browser/ui/views/intent_picker_bubble_view.cc created and the
// main component sizes were also mostly copied over to share the
// same layout.
// Main components sizes
constexpr int kIntentPickerCheckBoxColumnWidth = 288;
constexpr int kMaxIntentPickerWidth = 320;
constexpr int kRowHeight = 32;
constexpr int kTitlePadding = 16;
constexpr gfx::Insets kSeparatorPadding(0, 0, 16, 0);
constexpr SkColor kSeparatorColor = SkColorSetARGB(0x1F, 0x0, 0x0, 0x0);

std::unique_ptr<views::Separator> CreateHorizontalSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(kSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(kSeparatorPadding));
  return separator;
}

}  // namespace

// static
void WebAppProtocolHandlerIntentPickerView::Show(
    const GURL& url,
    Profile* profile,
    const base::CommandLine& command_line,
    std::vector<std::string> app_ids,
    base::OnceCallback<void(bool accepted)> close_callback) {
  std::unique_ptr<WebAppProtocolHandlerIntentPickerView> view =
      std::make_unique<WebAppProtocolHandlerIntentPickerView>(
          url, profile, command_line, std::move(app_ids),
          std::move(close_callback));
  WebAppProtocolHandlerIntentPickerView* view_ptr = view.get();

  views::DialogDelegate::CreateDialogWidget(std::move(view),
                                            /*context=*/nullptr,
                                            /*parent=*/nullptr)
      ->Show();

  // Set the first entry as the default selected App, can only be done
  // after Show();
  if (!view_ptr->hover_buttons_.empty()) {
    view_ptr->hover_buttons_[view_ptr->selected_app_tag_]->MarkAsSelected(
        nullptr);
    view_ptr->RequestFocus();
  }
}

WebAppProtocolHandlerIntentPickerView::WebAppProtocolHandlerIntentPickerView(
    const GURL& url,
    Profile* profile,
    const base::CommandLine& command_line,
    std::vector<std::string> app_ids,
    base::OnceCallback<void(bool accepted)> close_callback)
    : url_(url),
      profile_(profile),
      command_line_(command_line),
      app_ids_(std::move(app_ids)),
      close_callback_(std::move(close_callback)),
      // The ScopedKeepAlive ensures the process is alive until the dialog is
      // closed, and initiates the shutdown at closure if there is nothing
      // else keeping the browser alive.
      keep_alive_(std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::WEB_APP_INTENT_PICKER,
          KeepAliveRestartOption::DISABLED)) {
  SetDefaultButton(ui::DIALOG_BUTTON_OK);
  SetModalType(ui::MODAL_TYPE_WINDOW);
  std::u16string title =
      app_ids_.size() > 1
          ? l10n_util::GetStringUTF16(
                IDS_PROTOCOL_HANDLER_INTENT_PICKER_MULTI_TITLE)
          : l10n_util::GetStringUTF16(
                IDS_PROTOCOL_HANDLER_INTENT_PICKER_SINGLE_TITLE);
  SetTitle(title);
  SetShowCloseButton(false);

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_PROTOCOL_HANDLER_INTENT_PICKER_MULTI_OK_BUTTON_TEXT));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          IDS_PROTOCOL_HANDLER_INTENT_PICKER_MULTI_CANCEL_BUTTON_TEXT));

  SetAcceptCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnAccepted,
                     base::Unretained(this)));

  SetCancelCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnCanceled,
                     base::Unretained(this)));

  SetCloseCallback(
      base::BindOnce(&WebAppProtocolHandlerIntentPickerView::OnClosed,
                     base::Unretained(this)));
  Initialize();
}

WebAppProtocolHandlerIntentPickerView::
    ~WebAppProtocolHandlerIntentPickerView() = default;

gfx::Size WebAppProtocolHandlerIntentPickerView::CalculatePreferredSize()
    const {
  return gfx::Size(kMaxIntentPickerWidth,
                   GetHeightForWidth(kMaxIntentPickerWidth));
}

const std::string& WebAppProtocolHandlerIntentPickerView::GetSelectedAppId()
    const {
  DCHECK_LT(selected_app_tag_, hover_buttons_.size());
  return hover_buttons_[selected_app_tag_]->app_id();
}

void WebAppProtocolHandlerIntentPickerView::SetSelectedAppIndex(
    size_t index,
    const ui::Event& event) {
  DCHECK_GE(index, 0u);
  DCHECK_LT(index, hover_buttons_.size());
  hover_buttons_[selected_app_tag_]->MarkAsUnselected(nullptr);
  selected_app_tag_ = index;
  hover_buttons_[selected_app_tag_]->MarkAsSelected(&event);
  views::View::RequestFocus();
}

void WebAppProtocolHandlerIntentPickerView::OnAccepted() {
  RunCloseCallback(/*accepted=*/true);
}

void WebAppProtocolHandlerIntentPickerView::OnCanceled() {
  RunCloseCallback(/*accepted=*/false);
}

void WebAppProtocolHandlerIntentPickerView::OnClosed() {
  if (GetWidget()->closed_reason() ==
      views::Widget::ClosedReason::kAcceptButtonClicked) {
    OnAccepted();
  } else {
    OnCanceled();
  }
}

void WebAppProtocolHandlerIntentPickerView::Initialize() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Creates a view to hold the views for each app.
  auto scrollable_view = std::make_unique<views::View>();
  scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  if (app_ids_.size() == 1) {
    // We want the default to be 'Block' when we're requesting permission.
    SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);
    SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(
            IDS_PROTOCOL_HANDLER_INTENT_PICKER_SINGLE_OK_BUTTON_TEXT));
    SetButtonLabel(
        ui::DIALOG_BUTTON_CANCEL,
        l10n_util::GetStringUTF16(
            IDS_PROTOCOL_HANDLER_INTENT_PICKER_SINGLE_CANCEL_BUTTON_TEXT));
  }

  web_app::WebAppProvider* provider = web_app::WebAppProvider::Get(profile_);
  web_app::AppRegistrar& registrar = provider->registrar();
  hover_buttons_.reserve(app_ids_.size());
  for (size_t i = 0; i < app_ids_.size(); ++i) {
    const std::string& app_id = app_ids_[i];
    auto app_button = std::make_unique<WebAppHoverButton>(
        base::BindRepeating(
            &WebAppProtocolHandlerIntentPickerView::SetSelectedAppIndex,
            base::Unretained(this), i),
        app_id, provider, registrar.GetAppShortName(app_id),
        registrar.GetAppStartUrl(app_id));
    app_button->set_tag(i);
    hover_buttons_.push_back(app_button.get());
    scrollable_view->AddChildViewAt(std::move(app_button), i);
  }

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetBackgroundThemeColorId(
      ui::NativeTheme::kColorId_BubbleBackground);
  scroll_view->SetContents(std::move(scrollable_view));
  // This part gives the scroll a fixed width and height. The height depends on
  // how many app candidates we got and how many we actually want to show.
  // The added 0.5 on the else block allow us to let the user know there are
  // more than |kMaxAppResults| apps accessible by scrolling the list.
  scroll_view->ClipHeightTo(kRowHeight, (kMaxAppResults + 0.5) * kRowHeight);

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kFixed, kMaxIntentPickerWidth,
                0);

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnSetId,
                              views::GridLayout::kFixedSize, kTitlePadding);
  scroll_view_ = layout->AddView(std::move(scroll_view));
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId, 0);

  // The checkbox allows the user to opt-in to relaxed security
  // (i.e. skipping future prompts) for this url.
  layout->AddView(CreateHorizontalSeparator());
  // This second ColumnSet has a padding column in order to manipulate the
  // Checkbox positioning freely.
  constexpr int kColumnSetIdPadded = 2;
  views::ColumnSet* cs_padded = layout->AddColumnSet(kColumnSetIdPadded);
  cs_padded->AddPaddingColumn(views::GridLayout::kFixedSize, kTitlePadding);
  cs_padded->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::ColumnSize::kFixed,
                       kIntentPickerCheckBoxColumnWidth, 0);
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnSetIdPadded,
                              views::GridLayout::kFixedSize, 0);
  remember_selection_checkbox_ = layout->AddView(
      std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
          IDS_PROTOCOL_HANDLER_INTENT_PICKER_REMEMBER_SELECTION)));

  layout->AddPaddingRow(views::GridLayout::kFixedSize, kRowHeight);
}

void WebAppProtocolHandlerIntentPickerView::RunCloseCallback(bool accepted) {
  if (close_callback_) {
    std::move(close_callback_).Run(accepted);
  }
}

BEGIN_METADATA(WebAppProtocolHandlerIntentPickerView, views::DialogDelegateView)
END_METADATA

namespace chrome {

void ShowWebAppProtocolHandlerIntentPicker(
    const GURL& url,
    Profile* profile,
    const base::CommandLine& command_line,
    base::OnceCallback<void(bool accepted)> close_callback) {
  auto registry_ready_callback =
      [](const GURL& url, Profile* profile,
         const base::CommandLine& command_line,
         base::OnceCallback<void(bool accepted)> close_callback) {
        // TODO(crbug.com/1105257): Provide a list of installed web apps
        // app_ids.
        std::vector<std::string> app_ids;
        WebAppProtocolHandlerIntentPickerView::Show(url, profile, command_line,
                                                    std::move(app_ids),
                                                    std::move(close_callback));
      };

  auto* provider = web_app::WebAppProvider::Get(profile);
  DCHECK(provider);
  // Sometimes it is too early for registrar to be populated at this time. We
  // need to wait for it to get the web application info.
  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(std::move(registry_ready_callback), url, profile,
                     command_line, std::move(close_callback)));
}

}  // namespace chrome
