// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CERTIFICATE_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_CERTIFICATE_SELECTOR_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "net/ssl/client_cert_identity.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class LabelButton;
class TableView;
class View;
}

namespace ui {
class TableModel;
}

namespace chrome {

// A base class for dialogs that show a given list of certificates to the user.
// The user can select a single certificate and look at details of each
// certificate.
// The currently selected certificate can be obtained using |GetSelectedCert()|.
// The explanatory text shown to the user must be provided to |InitWithText()|.
class CertificateSelector : public views::DialogDelegateView,
                            public views::TableViewObserver {
  METADATA_HEADER(CertificateSelector, views::DialogDelegateView)

 public:
  // Indicates if the dialog can be successfully shown.
  // TODO(davidben): Remove this when the certificate selector prompt is moved
  // to the WebContentsDelegate. https://crbug.com/456255.
  static bool CanShow(content::WebContents* web_contents);

  // |web_contents| must not be null.
  CertificateSelector(net::ClientCertIdentityList identities,
                      content::WebContents* web_contents);
  CertificateSelector(const CertificateSelector&) = delete;
  CertificateSelector& operator=(const CertificateSelector&) = delete;
  ~CertificateSelector() override;

  // Handles when the user chooses a certificate in the list.
  // The CertificateSelector will be destroyed after this method completes.
  virtual void AcceptCertificate(
      std::unique_ptr<net::ClientCertIdentity> identity) = 0;

  // Returns the currently selected certificate or null if none is selected.
  // Must be called after |InitWithText()|.
  net::ClientCertIdentity* GetSelectedCert() const;

  // Shows this dialog as a constrained web modal dialog and focuses the first
  // certificate.
  // Must be called after |InitWithText()|.
  void Show();

  // DialogDelegateView:
  bool Accept() override;
  std::u16string GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;

 protected:
  // The dimensions of the certificate selector table view, in pixels.
  static const int kTableViewWidth;
  static const int kTableViewHeight;

  // Initializes the dialog. |text| is shown above the list of certificates
  // and is supposed to explain to the user what the implication of the
  // certificate selection is.
  void InitWithText(std::unique_ptr<views::View> text_label);

  ui::TableModel* table_model_for_testing() const;

 private:
  class CertificateTableModel;

  void ViewCertButtonPressed();

  net::ClientCertIdentityList identities_;

  // Whether to show the provider column in the table or not. Certificates
  // provided by the platform show the empty string as provider. That column is
  // shown only if there is at least one non-empty provider, i.e. non-platform
  // certificate.
  bool show_provider_column_ = false;
  std::unique_ptr<CertificateTableModel> model_;

  const raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      web_contents_;

  raw_ptr<views::TableView, DanglingUntriaged> table_ = nullptr;
  raw_ptr<views::LabelButton, DanglingUntriaged> view_cert_button_ = nullptr;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_CERTIFICATE_SELECTOR_H_
