// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_URL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_URL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/url_handler_launch_params.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class Profile;
class ScopedKeepAlive;
class WebAppUrlHandlerHoverButton;

namespace gfx {
class Size;
}

namespace ui {
class Event;
}

namespace views {
class Checkbox;
class ScrollView;
}  // namespace views

// The dialog's view, owned by the views framework.
// TODO(crbug.com/1209222): Dialog should be accessible.
class WebAppUrlHandlerIntentPickerView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(WebAppUrlHandlerIntentPickerView);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DialogState {
    kClosed = 0,
    kBrowserAcceptedAndRememberChoice = 1,
    kBrowserAcceptedNoRememberChoice = 2,
    kAppAcceptedAndRememberChoice = 3,
    kAppAcceptedNoRememberChoice = 4,
    kMaxValue = kAppAcceptedNoRememberChoice,
  };

  WebAppUrlHandlerIntentPickerView(
      const GURL& url,
      std::vector<web_app::UrlHandlerLaunchParams> launch_params_list,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      chrome::WebAppUrlHandlerAcceptanceCallback dialog_close_callback);
  WebAppUrlHandlerIntentPickerView(const WebAppUrlHandlerIntentPickerView&) =
      delete;
  WebAppUrlHandlerIntentPickerView& operator=(
      const WebAppUrlHandlerIntentPickerView&) = delete;
  ~WebAppUrlHandlerIntentPickerView() override;

  // Returns the set of profiles referenced by `launch_params_list` (loading
  // them if necessary) and removes any items in `launch_params_list` that
  // reference invalid or unloadable profiles.
  static base::flat_set<Profile*> GetUrlHandlingValidProfiles(
      std::vector<web_app::UrlHandlerLaunchParams>& launch_params_list);

  static void Show(
      const GURL& url,
      std::vector<web_app::UrlHandlerLaunchParams> launch_params_list,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      chrome::WebAppUrlHandlerAcceptanceCallback dialog_close_callback);

 private:
  using HoverButtons = std::vector<WebAppUrlHandlerHoverButton*>;

  void Initialize();
  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  // Return the UrlHandlerLaunchParams for the selected option. Null when the
  // browser is selected.
  absl::optional<web_app::UrlHandlerLaunchParams> GetSelectedLaunchParams()
      const;

  void OnAccepted();
  void OnCanceled();
  // Close callback called by DialogDeletegate. See
  // DialogDelegate::SetCloseCallback for when it's called.
  void OnClosed();

  // Unselects the current focused app item on the list and
  // refocus on the selected app item based on the index provided.
  void SetSelectedAppIndex(size_t index, const ui::Event& event);

  // Runs the close_callback_ provided during Show() if it exists.
  void RunCloseCallback(bool accepted);

  // Return if the |selected_app_tag_| is valid.
  bool IsSelectedAppValid() const;
  // Return if the user has selected an app in the dialog.
  bool HasUserSelectedApp() const;

  // The URL to launch if the dialog is accepted.
  const GURL url_;
  const std::vector<web_app::UrlHandlerLaunchParams> launch_params_list_;
  chrome::WebAppUrlHandlerAcceptanceCallback close_callback_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  HoverButtons hover_buttons_;
  // Allow the checkbox to be enabled or disabled. Enabled if the URL Handling
  // feature flag is enabled, disabled otherwise.
  // TODO(crbug.com/1072058): Remove when settings are implemented.
  bool enable_remember_checkbox_ = false;
  views::Checkbox* remember_selection_checkbox_ = nullptr;
  views::ScrollView* scroll_view_ = nullptr;

  // No default selection. Not null if selected by user.
  absl::optional<int> selected_app_tag_ = absl::nullopt;
};

BEGIN_VIEW_BUILDER(,
                   WebAppUrlHandlerIntentPickerView,
                   views::DialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, WebAppUrlHandlerIntentPickerView)

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_URL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_
