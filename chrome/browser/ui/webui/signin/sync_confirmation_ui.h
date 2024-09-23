// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"
#include "components/sync/base/user_selectable_type.h"

class Browser;
class Profile;

namespace content {
class WebUIDataSource;
}

namespace syncer {
class SyncService;
}

namespace content {
class WebUI;
}  // namespace content

enum class SyncConfirmationStyle;

// WebUI controller for the sync confirmation dialog.
//
// Note: This controller does not set the WebUI message handler. It is
// the responsibility of the caller to pass the correct message handler.
class SyncConfirmationUI : public SigninWebDialogUI {
 public:
  // Exposed for testing
  // Returns JSON data representing sync benefits that should be presented to
  // the user, based on which `syncer::UserSelectableType`s are available.
  // The data format is:
  // `[{"iconName": "${iron_icon_id}", "title": "${grit_string_id}"}, ...]`
  static std::string GetSyncBenefitsListJSON(
      const syncer::SyncService* sync_service);

  explicit SyncConfirmationUI(content::WebUI* web_ui);

  SyncConfirmationUI(const SyncConfirmationUI&) = delete;
  SyncConfirmationUI& operator=(const SyncConfirmationUI&) = delete;

  ~SyncConfirmationUI() override;

  // SigninWebDialogUI:
  // `browser` can be nullptr when the UI is displayed without a browser.
  void InitializeMessageHandlerWithBrowser(Browser* browser) override;

 private:
  void InitializeForSyncConfirmation(content::WebUIDataSource* source,
                                     SyncConfirmationStyle style,
                                     bool is_sync_promo);
  void InitializeForSyncDisabled(content::WebUIDataSource* source);

  // Adds a string resource with the given GRD |ids| to the WebUI data |source|
  // named as |name|. Also stores a reverse mapping from the localized version
  // of the string to the |ids| in order to later pass it to
  // SyncConfirmationHandler.
  void AddStringResource(content::WebUIDataSource* source,
                         const std::string& name,
                         int ids);

  // Adds a string resource with the given GRD |ids| and |parameter| as the
  // placeholder to the WebUI data |source| named as |name|. Also stores a
  // reverse mapping from the localized version of the string to the |ids| in
  // order to later pass it to SyncConfirmationHandler.
  void AddStringResourceWithPlaceholder(content::WebUIDataSource* source,
                                        const std::string& name,
                                        int ids,
                                        const std::u16string& parameter);

  // Adds a mapping from the localized version of a string |localized_string| to
  // its given GRD |ids| in order to later pass it to SyncConfirmationHandler.
  void AddLocalizedStringToIdsMap(const std::string& localized_string, int ids);

  // For consent auditing.
  std::unordered_map<std::string, int> js_localized_string_to_ids_map_;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_
