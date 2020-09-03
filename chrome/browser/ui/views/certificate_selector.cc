// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/certificate_selector.h"

#include <stddef.h>  // For size_t.
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#endif

namespace chrome {

const int CertificateSelector::kTableViewWidth = 500;
const int CertificateSelector::kTableViewHeight = 150;

class CertificateSelector::CertificateTableModel : public ui::TableModel {
 public:
  // |identities| and |provider_names| must have the same size.
  CertificateTableModel(const net::ClientCertIdentityList& identities,
                        const std::vector<std::string>& provider_names);

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int index, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

 private:
  struct Row {
    base::string16 subject;
    base::string16 issuer;
    base::string16 provider;
    base::string16 serial;
  };
  std::vector<Row> rows_;

  DISALLOW_COPY_AND_ASSIGN(CertificateTableModel);
};

CertificateSelector::CertificateTableModel::CertificateTableModel(
    const net::ClientCertIdentityList& identities,
    const std::vector<std::string>& provider_names) {
  DCHECK_EQ(identities.size(), provider_names.size());
  for (size_t i = 0; i < identities.size(); i++) {
    net::X509Certificate* cert = identities[i]->certificate();
    Row row;
    row.subject = base::UTF8ToUTF16(cert->subject().GetDisplayName());
    row.issuer = base::UTF8ToUTF16(cert->issuer().GetDisplayName());
    row.provider = base::UTF8ToUTF16(provider_names[i]);
    if (cert->serial_number().size() < std::numeric_limits<size_t>::max() / 2) {
      row.serial = base::UTF8ToUTF16(base::HexEncode(
          cert->serial_number().data(), cert->serial_number().size()));
    }
    rows_.push_back(row);
  }
}

int CertificateSelector::CertificateTableModel::RowCount() {
  return rows_.size();
}

base::string16 CertificateSelector::CertificateTableModel::GetText(
    int index,
    int column_id) {
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), rows_.size());

  const Row& row = rows_[index];
  switch (column_id) {
    case IDS_CERT_SELECTOR_SUBJECT_COLUMN:
      return row.subject;
    case IDS_CERT_SELECTOR_ISSUER_COLUMN:
      return row.issuer;
    case IDS_CERT_SELECTOR_PROVIDER_COLUMN:
      return row.provider;
    case IDS_CERT_SELECTOR_SERIAL_COLUMN:
      return row.serial;
    default:
      NOTREACHED();
  }
  return base::string16();
}

void CertificateSelector::CertificateTableModel::SetObserver(
    ui::TableModelObserver* observer) {}

CertificateSelector::CertificateSelector(net::ClientCertIdentityList identities,
                                         content::WebContents* web_contents)
    : web_contents_(web_contents) {
  SetCanResize(true);
  CHECK(web_contents_);

  view_cert_button_ = SetExtraView(std::make_unique<views::MdTextButton>(
      this, l10n_util::GetStringUTF16(IDS_PAGE_INFO_CERT_INFO_BUTTON)));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::CONTROL));

  // |provider_names| and |identities_| are parallel arrays.
  // The entry at index |i| is the provider name for |identities_[i]|.
  std::vector<std::string> provider_names;
#if defined(OS_CHROMEOS)
  chromeos::CertificateProviderService* service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistryFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());

  for (auto& identity : identities) {
    std::string provider_name;
    bool has_extension = false;
    std::string extension_id;
    if (service->LookUpCertificate(*identity->certificate(), &has_extension,
                                   &extension_id)) {
      if (!has_extension) {
        // This certificate was provided by an extension but isn't provided by
        // any extension currently. Don't expose it to the user.
        continue;
      }
      const auto* extension = extension_registry->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED);
      if (!extension) {
        // This extension was unloaded in the meantime. Don't show the
        // certificate.
        continue;
      }
      provider_name = extension->short_name();
      show_provider_column_ = true;
    }  // Otherwise the certificate is provided by the platform.

    identities_.push_back(std::move(identity));
    provider_names.push_back(provider_name);
  }
#else
  provider_names.assign(identities.size(), std::string());
  identities_ = std::move(identities);
#endif

  model_ = std::make_unique<CertificateTableModel>(identities_, provider_names);
}

CertificateSelector::~CertificateSelector() {
  table_->SetModel(nullptr);
}

// static
bool CertificateSelector::CanShow(content::WebContents* web_contents) {
  // GetTopLevelWebContents returns |web_contents| if it is not a guest.
  content::WebContents* top_level_web_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(web_contents);
  return web_modal::WebContentsModalDialogManager::FromWebContents(
             top_level_web_contents) != nullptr;
}

void CertificateSelector::Show() {
  constrained_window::ShowWebModalDialogViews(this, web_contents_);

  // TODO(isandrk): A certificate that was previously provided by *both* the
  // platform and an extension will get incorrectly filtered out if the
  // extension stops providing it (both instances will be filtered out), hence
  // the |identities_| array will be empty. Displaying a dialog with an empty
  // list won't make much sense for the user, and also there are some CHECKs in
  // the code that will fail when the list is empty and that's why an early exit
  // is performed here. See crbug.com/641440 for more details.
  if (identities_.empty()) {
    GetWidget()->Close();
    return;
  }

  // Select the first row automatically.  This must be done after the dialog has
  // been created.
  table_->Select(0);
}

void CertificateSelector::InitWithText(
    std::unique_ptr<views::View> text_label) {
  views::GridLayout* const layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  const int kColumnSetId = 0;
  views::ColumnSet* const column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  layout->AddView(std::move(text_label));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int vertical_spacing = provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);

  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn(IDS_CERT_SELECTOR_SUBJECT_COLUMN,
                                    ui::TableColumn::LEFT, -1, 0.4f));
  columns.push_back(ui::TableColumn(IDS_CERT_SELECTOR_ISSUER_COLUMN,
                                    ui::TableColumn::LEFT, -1, 0.2f));
  if (show_provider_column_) {
    columns.push_back(ui::TableColumn(IDS_CERT_SELECTOR_PROVIDER_COLUMN,
                                      ui::TableColumn::LEFT, -1, 0.4f));
  }
  columns.push_back(ui::TableColumn(IDS_CERT_SELECTOR_SERIAL_COLUMN,
                                    ui::TableColumn::LEFT, -1, 0.2f));
  auto table = std::make_unique<views::TableView>(
      model_.get(), columns, views::TEXT_ONLY, true /* single_selection */);
  table_ = table.get();
  table->set_observer(this);
  layout->StartRow(1.0, kColumnSetId);
  layout->AddView(views::TableView::CreateScrollViewWithTable(std::move(table)),
                  1, 1, views::GridLayout::FILL, views::GridLayout::FILL,
                  kTableViewWidth, kTableViewHeight);

  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);
}

ui::TableModel* CertificateSelector::table_model_for_testing() const {
  return model_.get();
}

net::ClientCertIdentity* CertificateSelector::GetSelectedCert() const {
  const int selected = table_->GetFirstSelectedRow();
  if (selected < 0)  // Nothing is selected in |table_|.
    return nullptr;
  DCHECK_LT(static_cast<size_t>(selected), identities_.size());
  return identities_[selected].get();
}

bool CertificateSelector::Accept() {
  const int selected = table_->GetFirstSelectedRow();
  if (selected < 0)  // Nothing is selected in |table_|.
    return false;

  DCHECK_LT(static_cast<size_t>(selected), identities_.size());
  AcceptCertificate(std::move(identities_[selected]));
  return true;
}

base::string16 CertificateSelector::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CLIENT_CERT_DIALOG_TITLE);
}

bool CertificateSelector::IsDialogButtonEnabled(ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK || GetSelectedCert();
}

views::View* CertificateSelector::GetInitiallyFocusedView() {
  DCHECK(table_);
  return table_;
}

ui::ModalType CertificateSelector::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void CertificateSelector::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == view_cert_button_) {
    net::ClientCertIdentity* const cert = GetSelectedCert();
    if (cert) {
      ShowCertificateViewer(web_contents_,
                            web_contents_->GetTopLevelNativeWindow(),
                            cert->certificate());
    }
  }
}

void CertificateSelector::OnSelectionChanged() {
  GetOkButton()->SetEnabled(GetSelectedCert() != nullptr);
}

void CertificateSelector::OnDoubleClick() {
  if (GetSelectedCert())
    AcceptDialog();
}

}  // namespace chrome
