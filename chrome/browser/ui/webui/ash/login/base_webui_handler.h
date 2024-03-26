// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_BASE_WEBUI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_BASE_WEBUI_HANDLER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "components/login/base_screen_handler_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace login {
class LocalizedValuesBuilder;
}

namespace ash {

class OobeUI;

// Base class for all oobe/login WebUI handlers. These handlers are the binding
// layer that allow the C++ and JavaScript code to communicate.
//
// If the deriving type is associated with a specific OobeScreen, it should
// derive from BaseScreenHandler instead of BaseWebUIHandler.
class BaseWebUIHandler : public content::WebUIMessageHandler {
 public:
  BaseWebUIHandler();

  BaseWebUIHandler(const BaseWebUIHandler&) = delete;
  BaseWebUIHandler& operator=(const BaseWebUIHandler&) = delete;

  ~BaseWebUIHandler() override;

  // Gets localized strings to be used on the page.
  void GetLocalizedStrings(base::Value::Dict* localized_strings);

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;
  void OnJavascriptAllowed() final;

 protected:
  // All subclasses should implement this method to provide localized values.
  virtual void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) = 0;

  // All subclasses should implement this method to register callbacks for JS
  // messages.
  virtual void DeclareJSCallbacks() {}

  // Subclasses can override these methods to pass additional parameters
  // to loadTimeData.
  virtual void GetAdditionalParameters(base::Value::Dict* parameters);

  // Can be overridden to do any initialization after javascript is ready.
  virtual void InitAfterJavascriptAllowed();

  // Run a JavaScript function. If the backing webui that this handler is not
  // fully loaded, then the JS call will be deferred and executed after the
  // initialize message.
  //
  // All CallJS invocations can be recorded for tests if CallJS recording is
  // enabled.
  template <typename... Args>
  void CallJS(std::string_view function_name, Args... args) {
    if (IsJavascriptAllowed()) {
      CallJavascriptFunction(function_name, base::Value(std::move(args))...);
      return;
    }

    deferred_calls_.push_back(base::BindOnce(
        &BaseWebUIHandler::CallJS<Args...>, base::Unretained(this),
        std::string(function_name.data(), function_name.length()),
        std::move(args)...));
  }

  template <typename... Args>
  void FireWebUIListenerWhenAllowed(std::string_view event_name, Args... args) {
    if (IsJavascriptAllowed()) {
      FireWebUIListener(event_name, args...);
      return;
    }

    deferred_calls_.push_back(
        base::BindOnce(&BaseWebUIHandler::FireWebUIListenerWhenAllowed<Args...>,
                       base::Unretained(this),
                       std::string(event_name.data(), event_name.length()),
                       std::move(args)...));
  }

  template <typename T, typename... Args>
  void AddCallback(std::string_view function_name, void (T::*method)(Args...)) {
    base::RepeatingCallback<void(Args...)> callback =
        base::BindRepeating(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterHandlerCallback(function_name, callback);
  }

  // Show selected WebUI `screen`.
  void ShowScreenDeprecated(OobeScreenId screen);

  // Returns the OobeUI instance.
  OobeUI* GetOobeUI();

  // Returns current visible OOBE screen.
  OobeScreenId GetCurrentScreen();

 private:
  friend class OobeUI;

  std::vector<base::OnceClosure> deferred_calls_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_BASE_WEBUI_HANDLER_H_
