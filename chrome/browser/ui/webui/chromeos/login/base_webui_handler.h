// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"
#include "components/login/base_screen_handler_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace login {
class LocalizedValuesBuilder;
}

namespace chromeos {

namespace {

template <typename... Args>
void PostponedJSCall(const std::string& function_name,
                     Args... args,
                     content::WebUI* web_ui) {
  web_ui->CallJavascriptFunctionUnsafe(function_name,
                                       base::Value(std::move(args))...);
}

}  // namespace

class OobeUI;

// Base class for all oobe/login WebUI handlers. These handlers are the binding
// layer that allow the C++ and JavaScript code to communicate.
//
// If the deriving type is associated with a specific OobeScreen, it should
// derive from BaseScreenHandler instead of BaseWebUIHandler.
class BaseWebUIHandler : public content::WebUIMessageHandler {
 public:
  explicit BaseWebUIHandler(JSCallsContainer* js_calls_container);

  BaseWebUIHandler(const BaseWebUIHandler&) = delete;
  BaseWebUIHandler& operator=(const BaseWebUIHandler&) = delete;

  ~BaseWebUIHandler() override;

  // Gets localized strings to be used on the page.
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // This method is called when page is ready. It propagates to inherited class
  // via virtual Initialize() method (see below).
  void InitializeBase();

 protected:
  // All subclasses should implement this method to provide localized values.
  virtual void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) = 0;

  // All subclasses should implement this method to register callbacks for JS
  // messages.
  virtual void DeclareJSCallbacks() {}

  // Subclasses can override these methods to pass additional parameters
  // to loadTimeData.
  virtual void GetAdditionalParameters(base::DictionaryValue* parameters);

  // Run a JavaScript function. If the backing webui that this handler is not
  // fully loaded, then the JS call will be deferred and executed after the
  // initialize message.
  //
  // All CallJS invocations can be recorded for tests if CallJS recording is
  // enabled.
  template <typename... Args>
  void CallJS(const std::string& function_name, Args... args) {
    // Record the call if the WebUI is not loaded or if we are in a test.
    if (!js_calls_container_->is_initialized()) {
      js_calls_container_->events()->push_back(base::BindOnce(
          &PostponedJSCall<Args...>, function_name, std::move(args)...));
      return;
    }

    // Make the call now if the WebUI is loaded.
    if (js_calls_container_->is_initialized() && !javascript_disallowed_)
      web_ui()->CallJavascriptFunctionUnsafe(function_name,
                                             base::Value(std::move(args))...);
  }

  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed
  // If the JS container hasn't been initialized yet, it is safe to call JS
  // because the call will be postponed until we receive a message from the
  // renderer.
  bool IsSafeToCallJavascript() {
    return (js_calls_container_ && !js_calls_container_->is_initialized()) ||
           IsJavascriptAllowed();
  }

  // Register WebUI callbacks. The callbacks will be recorded if recording is
  // enabled.
  template <typename T>
  void AddRawCallback(const std::string& function_name,
                      void (T::*method)(const base::ListValue* args)) {
    content::WebUI::DeprecatedMessageCallback callback =
        base::BindRepeating(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterDeprecatedMessageCallback(
        function_name,
        base::BindRepeating(&BaseWebUIHandler::OnRawCallback,
                            base::Unretained(this), function_name, callback));
  }
  template <typename T, typename... Args>
  void AddCallback(const std::string& function_name,
                   void (T::*method)(Args...)) {
    base::RepeatingCallback<void(Args...)> callback =
        base::BindRepeating(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterDeprecatedMessageCallback(
        function_name,
        base::BindRepeating(&BaseWebUIHandler::OnCallback<Args...>,
                            base::Unretained(this), function_name, callback));
  }

  // Called when the page is ready and handler can do initialization.
  virtual void Initialize() = 0;

  // Show selected WebUI `screen`.
  void ShowScreen(OobeScreenId screen);
  // Show selected WebUI `screen`. Pass screen initialization using the `data`
  // parameter.
  void ShowScreenWithData(OobeScreenId screen,
                          const base::DictionaryValue* data);

  // Returns the OobeUI instance.
  OobeUI* GetOobeUI();

  // Returns current visible OOBE screen.
  OobeScreenId GetCurrentScreen();

  // Whether page is ready.
  bool page_is_ready() const { return page_is_ready_; }

  // content::WebUIMessageHandler
  void OnJavascriptDisallowed() override;

 private:
  friend class OobeUI;

  // These two functions wrap Add(Raw)Callback so that the incoming JavaScript
  // event can be recorded.
  void OnRawCallback(const std::string& function_name,
                     const content::WebUI::DeprecatedMessageCallback& callback,
                     const base::ListValue* args);
  template <typename... Args>
  void OnCallback(const std::string& function_name,
                  const base::RepeatingCallback<void(Args...)>& callback,
                  const base::ListValue* args) {
    ::login::CallbackWrapper<Args...>(callback, args);
  }

  // Keeps whether page is ready.
  bool page_is_ready_ = false;

  // When there isn't a RenderFrameHost, no JS calls should be made.
  // This flag becomes true when the renderer is destroyed.
  // TODO(crbug/1180291) - Remove this custom solution once OOBE's JS
  // initialisation has been fixed.
  bool javascript_disallowed_ = false;

  JSCallsContainer* js_calls_container_ = nullptr;  // non-owning pointers.
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_
