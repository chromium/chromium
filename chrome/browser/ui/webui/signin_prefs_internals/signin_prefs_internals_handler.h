// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PREFS_INTERNALS_SIGNIN_PREFS_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PREFS_INTERNALS_SIGNIN_PREFS_INTERNALS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

// Backs the Sign-in prefs tab of chrome://signin-internals. Handles the
// following `cr.sendWithPromise()` messages (see README.md for the contract):
// - `getSigninPrefs()`: the current profile's per-account pref values, the
//   emails of the accounts that may have signin prefs, and `canEdit`.
// - `getSupportedSigninPrefs()`: the schema of the registered prefs, as a list
//   of `{parentKeys, key, type}`.
// - `setSigninPref(gaiaId, parentKeys, key, value)`: sets a registered pref,
//   and resolves with no value.
// - `deleteSigninPref(gaiaId, parentKeys, key)`: deletes a registered pref,
//   and resolves with whether it was set.
//
// All inputs come from the renderer and are validated against
// `SigninPrefsRegistry` before reaching `SigninPrefsAccessor`, so no renderer
// input can trigger an accessor `CHECK`. The promise of a message with inputs
// the page cannot send is rejected with an error string, and so is the promise
// of a mutation while editing is disabled (`"Editing is disabled"`).
class SigninPrefsInternalsHandler : public content::WebUIMessageHandler {
 public:
  // The value type of a registered signin pref, sent to the page as an
  // integer by `getSupportedSigninPrefs()`. The page mirrors these values, so
  // they must not change.
  // TODO(crbug.com/555954301): Add LINT.IfChange once browser_proxy.ts is added
  // in the next CL.
  enum class PrefType {
    kBoolean = 0,
    kInteger = 1,
    kString = 2,
    // A string holding a `base::Time`, serialized as the number of
    // microseconds since the Windows epoch (see `base::TimeToValue()`).
    kTimestamp = 3,
    // A dictionary that only groups other registered prefs. It cannot be set
    // directly, but deleting it removes all the prefs it contains.
    kDictionary = 4,
  };

  explicit SigninPrefsInternalsHandler(Profile* profile);
  ~SigninPrefsInternalsHandler() override;

  SigninPrefsInternalsHandler(const SigninPrefsInternalsHandler&) = delete;
  SigninPrefsInternalsHandler& operator=(const SigninPrefsInternalsHandler&) =
      delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Only this class can mint the key granting access to `SigninPrefsRegistry`
  // and `SigninPrefsAccessor`.
  using PassKey = base::PassKey<SigninPrefsInternalsHandler>;

  // Message handlers. `args[0]` is the `cr.sendWithPromise()` callback ID.
  // Lists the current profile's per-account pref values and known accounts.
  void HandleGetSigninPrefs(const base::ListValue& args);
  // Lists the schema of available registered signin prefs.
  void HandleGetSupportedSigninPrefs(const base::ListValue& args);
  void HandleSetSigninPref(const base::ListValue& args);
  void HandleDeleteSigninPref(const base::ListValue& args);

  bool CanEdit() const;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PREFS_INTERNALS_SIGNIN_PREFS_INTERNALS_HANDLER_H_
