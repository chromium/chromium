// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"

#include <vector>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout_view.h"

namespace ash {

namespace {

// This dialog is used in place of the browser http auth dialog when
// `g_enable_count` >= 1. Once Lacros ships, this feature can be enabled always.
static int g_enable_count = 0;

// The distance between vertical controls.
constexpr int kDistanceControlListVertical = 12;

// All HttpAuthDialogs should be tracked in this global singleton for testing.
using HttpAuthDialogVector = std::vector<HttpAuthDialog*>;
HttpAuthDialogVector& GetAllDialogs() {
  static base::NoDestructor<HttpAuthDialogVector> instance;
  return *instance;
}

// All observers should be tracked in this global singleton.
using Observers = base::ObserverList<HttpAuthDialog::Observer>;
Observers& GetObservers() {
  static base::NoDestructor<Observers> instance;
  return *instance;
}

// Computes `authority` and `explanation`.
void GetDialogStrings(const GURL& request_url,
                      const net::AuthChallengeInfo& auth_info,
                      std::u16string* authority,
                      std::u16string* explanation) {
  GURL authority_url;

  if (auth_info.is_proxy) {
    *authority = l10n_util::GetStringFUTF16(
        IDS_LOGIN_DIALOG_PROXY_AUTHORITY,
        url_formatter::FormatUrlForSecurityDisplay(
            auth_info.challenger.GetURL(), url_formatter::SchemeDisplay::SHOW));
    authority_url = auth_info.challenger.GetURL();
  } else {
    *authority = url_formatter::FormatUrlForSecurityDisplay(request_url);
    authority_url = request_url;
  }

  if (!network::IsUrlPotentiallyTrustworthy(authority_url)) {
    *explanation = l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_NOT_PRIVATE);
  } else {
    explanation->clear();
  }
}

}  // namespace

HttpAuthDialog::~HttpAuthDialog() {
  // Book-keeping for test-only data-structures.
  auto& dialogs = GetAllDialogs();
  auto it = std::find(dialogs.begin(), dialogs.end(), this);
  DCHECK(it != dialogs.end());
  dialogs.erase(it);

  // The widget will be destroyed soon, so we must first clear raw_ptrs owned by
  // the widget.
  dialog_view_ = nullptr;
}

HttpAuthDialog::ScopedEnabler::ScopedEnabler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ++g_enable_count;
}

HttpAuthDialog::ScopedEnabler::~ScopedEnabler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  --g_enable_count;
}

std::unique_ptr<HttpAuthDialog::ScopedEnabler> HttpAuthDialog::Enable() {
  return std::make_unique<ScopedEnabler>();
}

// static
bool HttpAuthDialog::IsEnabled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_enable_count >= 1;
}

// static
std::unique_ptr<HttpAuthDialog> HttpAuthDialog::Create(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const GURL& url,
    LoginAuthRequiredCallback auth_required_callback) {
  // This class cannot handle UI-less auth dialog requests. Once Lacros ships,
  // this should no longer be possible and this can become a CHECK.
  if (!web_contents) {
    return nullptr;
  }

  // Anchor to the outermost WebContents, for e.g. embedded <webview>s.
  web_contents = web_contents->GetOutermostWebContents();

  // Skip if the WebContents instance is not prepared to show a dialog.
  if (!web_modal::WebContentsModalDialogManager::FromWebContents(
          web_contents)) {
    LOG(ERROR) << "Skipping HttpAuthDialog, url=" << url.possibly_invalid_spec()
               << ", web_contents?" << !!web_contents;
    base::debug::DumpWithoutCrashing();
    return nullptr;
  }

  // The constructor is private. There is no portable way to expose the
  // constructor to std::make_unique.
  return base::WrapUnique(new HttpAuthDialog(
      auth_info, web_contents, url, std::move(auth_required_callback)));
}

// static
void HttpAuthDialog::AddObserver(Observer* observer) {
  GetObservers().AddObserver(observer);
}

// static
void HttpAuthDialog::RemoveObserver(Observer* observer) {
  GetObservers().RemoveObserver(observer);
}

// static
std::vector<HttpAuthDialog*> HttpAuthDialog::GetAllDialogsForTest() {
  return GetAllDialogs();
}

void HttpAuthDialog::SupplyCredentialsForTest(std::u16string_view username,
                                              std::u16string_view password) {
  dialog_view_->SetCredentialsForTest(std::move(username), std::move(password));
  dialog_delegate_.AcceptDialog();
}

void HttpAuthDialog::CancelForTest() {
  dialog_delegate_.CancelDialog();
}

HttpAuthDialog::DialogView::DialogView(std::u16string_view authority,
                                       std::u16string_view explanation) {
  std::u16string authority_string(authority);
  std::u16string explanation_string(explanation);
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kControl),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  auto* authority_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  authority_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  auto* authority_label =
      authority_container->AddChildView(std::make_unique<views::Label>(
          authority_string, views::style::CONTEXT_LABEL,
          views::style::STYLE_PRIMARY));
  authority_label->SetMultiLine(true);
  constexpr int kMessageWidth = 320;
  authority_label->SetMaximumWidth(kMessageWidth);
  authority_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  authority_label->SetAllowCharacterBreak(true);
  if (!explanation_string.empty()) {
    auto* explanation_label =
        authority_container->AddChildView(std::make_unique<views::Label>(
            explanation_string, views::style::CONTEXT_LABEL,
            views::style::STYLE_SECONDARY));
    explanation_label->SetMultiLine(true);
    explanation_label->SetMaximumWidth(kMessageWidth);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  auto* fields_container =
      AddChildView(std::make_unique<views::TableLayoutView>());
  fields_container
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(
          views::TableLayout::kFixedSize,
          provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, 1.0,
                 views::TableLayout::ColumnSize::kFixed, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(views::TableLayout::kFixedSize,
                     kDistanceControlListVertical)
      .AddRows(1, views::TableLayout::kFixedSize);
  auto* username_label =
      fields_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_USERNAME_FIELD),
          views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  username_field_ =
      fields_container->AddChildView(std::make_unique<views::Textfield>());
  username_field_->GetViewAccessibility().SetName(*username_label);
  auto* password_label =
      fields_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_PASSWORD_FIELD),
          views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  password_field_ =
      fields_container->AddChildView(std::make_unique<views::Textfield>());
  password_field_->GetViewAccessibility().SetName(*password_label);
  password_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
}

HttpAuthDialog::DialogView::~DialogView() = default;

// Access the data in the username/password text fields.
std::u16string HttpAuthDialog::DialogView::GetUsername() const {
  return username_field_->GetText();
}

std::u16string HttpAuthDialog::DialogView::GetPassword() const {
  return password_field_->GetText();
}

void HttpAuthDialog::DialogView::SetCredentialsForTest(
    std::u16string_view username,
    std::u16string_view password) {
  std::u16string username_string(username);
  std::u16string password_string(password);
  username_field_->SetText(username_string);
  password_field_->SetText(password_string);
}

views::View* HttpAuthDialog::DialogView::GetInitiallyFocusedView() {
  return username_field_;
}

HttpAuthDialog::HttpAuthDialog(const net::AuthChallengeInfo& auth_info,
                               content::WebContents* web_contents,
                               const GURL& url,
                               LoginAuthRequiredCallback auth_required_callback)
    : callback_(std::move(auth_required_callback)),
      web_contents_(web_contents) {
  CHECK(!callback_.is_null());
  GetAllDialogs().push_back(this);

  dialog_delegate_.SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL));

  dialog_delegate_.SetAcceptCallback(base::BindOnce(
      [](base::WeakPtr<HttpAuthDialog> dialog) {
        if (!dialog) {
          return;
        }

        dialog->SupplyCredentials(dialog->dialog_view_->GetUsername(),
                                  dialog->dialog_view_->GetPassword());
      },
      weak_factory_.GetWeakPtr()));

  auto close_callback =
      base::BindOnce(&HttpAuthDialog::Cancel, weak_factory_.GetWeakPtr());

  // WindowClosing callback is guaranteed to be called regardless of whether the
  // dialog is closed by the user or the OS.
  dialog_delegate_.RegisterWindowClosingCallback(std::move(close_callback));
  dialog_delegate_.SetOwnershipOfNewWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  dialog_delegate_.SetModalType(ui::mojom::ModalType::kChild);
  dialog_delegate_.SetShowCloseButton(false);
  dialog_delegate_.SetTitle(l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE));

  std::u16string authority;
  std::u16string explanation;
  GetDialogStrings(url, auth_info, &authority, &explanation);
  dialog_view_ = dialog_delegate_.SetContentsView(
      std::make_unique<DialogView>(authority, explanation));
  dialog_delegate_.SetInitiallyFocusedView(
      dialog_view_->GetInitiallyFocusedView());

  dialog_widget_ = constrained_window::ShowWebModalDialogViewsOwned(
      &dialog_delegate_, web_contents,
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  NotifyShownAsync(web_contents_);
}

void HttpAuthDialog::SupplyCredentials(std::u16string_view username,
                                       std::u16string_view password) {
  std::u16string username_string(username);
  std::u16string password_string(password);
  net::AuthCredentials credentials(username_string, password_string);
  CHECK(!callback_.is_null());
  NotifySuppliedAsync(web_contents_);

  // Running `callback_` can result in synchronous destruction of this object.
  // We dispatch the call to avoid re-entrancy, as this method itself can be
  // synchronously invoked as a callback.
  auto run_callback = base::BindOnce(
      [](base::WeakPtr<HttpAuthDialog> dialog,
         LoginAuthRequiredCallback callback, net::AuthCredentials credentials) {
        if (dialog) {
          std::move(callback).Run(std::move(credentials));
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback_), std::move(credentials));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(run_callback));
}

void HttpAuthDialog::Cancel() {
  NotifyCancelledAsync(web_contents_);

  // Running `callback_` can result in synchronous destruction of this object.
  // We dispatch the call to avoid re-entrancy, as this method itself can be
  // synchronously invoked as a callback.
  auto run_callback = base::BindOnce(
      [](base::WeakPtr<HttpAuthDialog> dialog,
         LoginAuthRequiredCallback callback) {
        if (dialog) {
          std::move(callback).Run(std::nullopt);
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback_));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(run_callback));
}

// static
void HttpAuthDialog::NotifyShownAsync(content::WebContents* web_contents) {
  auto callback = base::BindOnce(
      [](base::WeakPtr<content::WebContents> web_contents) {
        for (auto& observer : GetObservers()) {
          observer.HttpAuthDialogShown(web_contents ? web_contents.get()
                                                    : nullptr);
        }
      },
      web_contents->GetWeakPtr());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

//  static
void HttpAuthDialog::NotifySuppliedAsync(content::WebContents* web_contents) {
  auto callback = base::BindOnce(
      [](base::WeakPtr<content::WebContents> web_contents) {
        for (auto& observer : GetObservers()) {
          observer.HttpAuthDialogSupplied(web_contents ? web_contents.get()
                                                       : nullptr);
        }
      },
      web_contents->GetWeakPtr());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

//  static
void HttpAuthDialog::NotifyCancelledAsync(content::WebContents* web_contents) {
  auto callback = base::BindOnce(
      [](base::WeakPtr<content::WebContents> web_contents) {
        for (auto& observer : GetObservers()) {
          observer.HttpAuthDialogCancelled(web_contents ? web_contents.get()
                                                        : nullptr);
        }
      },
      web_contents->GetWeakPtr());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

}  // namespace ash
