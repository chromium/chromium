// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/gfx/image/image.h"

namespace favicon_base {
struct FaviconImageResult;
}

namespace gfx {
class RectF;
}

namespace password_manager {

class PasswordManagerClient;
class PasswordManagerDriver;

// This class is responsible for filling password forms.
class PasswordAutofillManager : public autofill::AutofillPopupDelegate {
 public:
  PasswordAutofillManager(PasswordManagerDriver* password_manager_driver,
                          autofill::AutofillClient* autofill_client,
                          PasswordManagerClient* password_client);
  virtual ~PasswordAutofillManager();

  // AutofillPopupDelegate implementation.
  void OnPopupShown() override;
  void OnPopupHidden() override;
  void OnPopupSuppressed() override;
  void DidSelectSuggestion(const base::string16& value,
                           int identifier) override;
  void DidAcceptSuggestion(const base::string16& value,
                           int identifier,
                           int position) override;
  bool GetDeletionConfirmationText(const base::string16& value,
                                   int identifier,
                                   base::string16* title,
                                   base::string16* body) override;
  bool RemoveSuggestion(const base::string16& value, int identifier) override;
  void ClearPreviewedForm() override;
  autofill::PopupType GetPopupType() const override;
  autofill::AutofillDriver* GetAutofillDriver() override;
  int32_t GetWebContentsPopupControllerAxId() const override;
  void RegisterDeletionCallback(base::OnceClosure deletion_callback) override;

  // Invoked when a password mapping is added.
  void OnAddPasswordFillData(const autofill::PasswordFormFillData& fill_data);

  // Removes the credentials previously saved via OnAddPasswordFormMapping.
  void DeleteFillData();

  // Handles a request from the renderer to show a popup with the suggestions
  // from the password manager. |options| should be a bitwise mask of
  // autofill::ShowPasswordSuggestionsOptions values.
  void OnShowPasswordSuggestions(base::i18n::TextDirection text_direction,
                                 const base::string16& typed_username,
                                 int options,
                                 const gfx::RectF& bounds);

  // If there are relevant credentials for the current frame show them and
  // return true. Otherwise, return false.
  // This is currently used for cases in which the automatic generation
  // option is offered through a different UI surface than the popup
  // (e.g. via the keyboard accessory on Android).
  bool MaybeShowPasswordSuggestions(const gfx::RectF& bounds,
                                    base::i18n::TextDirection text_direction);

  // If there are relevant credentials for the current frame, shows them with
  // an additional 'generation' option and returns true. Otherwise, does nothing
  // and returns false.
  bool MaybeShowPasswordSuggestionsWithGeneration(
      const gfx::RectF& bounds,
      base::i18n::TextDirection text_direction,
      bool show_password_suggestions);

  // Called when main frame navigates. Not called for in-page navigations.
  void DidNavigateMainFrame();

  // A public version of FillSuggestion(), only for use in tests.
  bool FillSuggestionForTest(const base::string16& username);

  // A public version of PreviewSuggestion(), only for use in tests.
  bool PreviewSuggestionForTest(const base::string16& username);

#if defined(UNIT_TEST)
  void set_autofill_client(autofill::AutofillClient* autofill_client) {
    autofill_client_ = autofill_client;
  }
#endif  // defined(UNIT_TEST)

 private:
  // Attempts to fill the password associated with user name |username|, and
  // returns true if it was successful.
  bool FillSuggestion(const base::string16& username);

  // Attempts to preview the password associated with user name |username|, and
  // returns true if it was successful.
  bool PreviewSuggestion(const base::string16& username);

  // If |current_username| matches a username for one of the login mappings in
  // |fill_data|, returns true and assigns the password and the original signon
  // realm to |password_and_meta_data|. Note that if the credential comes from
  // the same realm as the one we're filling to, the |realm| field will be left
  // empty, as this is the behavior of |PasswordFormFillData|.
  // Otherwise, returns false and leaves |password_and_meta_data| untouched.
  bool GetPasswordAndMetadataForUsername(
      const base::string16& current_username,
      const autofill::PasswordFormFillData& fill_data,
      autofill::PasswordAndMetadata* password_and_meta_data);

  // Makes a request to the favicon service for the icon of |url|.
  void RequestFavicon(const GURL& url);

  // Called when the favicon was retrieved. When the icon is not ready or
  // unavailable a fallback globe icon is used. The request to the favicon
  // store is canceled on navigation.
  void OnFaviconReady(const favicon_base::FaviconImageResult& result);

  std::unique_ptr<autofill::PasswordFormFillData> fill_data_;

  // Contains the favicon for the credentials offered on the current page.
  gfx::Image page_favicon_;

  // The driver that owns |this|.
  PasswordManagerDriver* password_manager_driver_;

  autofill::AutofillClient* autofill_client_;  // weak

  PasswordManagerClient* password_client_;

  // If not null then it will be called in destructor.
  base::OnceClosure deletion_callback_;

  // Used to track a requested favicon.
  base::CancelableTaskTracker favicon_tracker_;

  base::WeakPtrFactory<PasswordAutofillManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
