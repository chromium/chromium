// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"
#include "components/login/base_screen_handler_utils.h"
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

class OobeUI;

// Base class for all oobe/login WebUI handlers. These handlers are the binding
// layer that allow the C++ and JavaScript code to communicate.
//
// If the deriving type is associated with a specific OobeScreen, it should
// derive from BaseScreenHandler instead of BaseWebUIHandler.
class BaseWebUIHandler : public content::WebUIMessageHandler {
 public:
  explicit BaseWebUIHandler(JSCallsContainer* js_calls_container);
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
  void CallJS(const std::string& function_name, const Args&... args) {
    // Record the call if the WebUI is not loaded or if we are in a test.
    if (!js_calls_container_->is_initialized() ||
        js_calls_container_->record_all_events_for_test()) {
      std::vector<base::Value> arguments;
      InsertIntoList(&arguments, args...);
      js_calls_container_->events()->emplace_back(
          JSCallsContainer::Event(JSCallsContainer::Event::Type::kOutgoing,
                                  function_name, std::move(arguments)));
    }

    // Make the call now if the WebUI is loaded.
    if (js_calls_container_->is_initialized())
      web_ui()->CallJavascriptFunctionUnsafe(
          function_name, ::login::MakeValue(args).Clone()...);
  }

  // Register WebUI callbacks. The callbacks will be recorded if recording is
  // enabled.
  template <typename T>
  void AddRawCallback(const std::string& function_name,
                      void (T::*method)(const base::ListValue* args)) {
    content::WebUI::MessageCallback callback =
        base::BindRepeating(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        function_name,
        base::BindRepeating(&BaseWebUIHandler::OnRawCallback,
                            base::Unretained(this), function_name, callback));
  }
  template <typename T, typename... Args>
  void AddCallback(const std::string& function_name,
                   void (T::*method)(Args...)) {
    base::RepeatingCallback<void(Args...)> callback =
        base::BindRepeating(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        function_name,
        base::BindRepeating(&BaseWebUIHandler::OnCallback<Args...>,
                            base::Unretained(this), function_name, callback));
  }

  // Called when the page is ready and handler can do initialization.
  virtual void Initialize() = 0;

  // Show selected WebUI |screen|.
  void ShowScreen(OobeScreenId screen);
  // Show selected WebUI |screen|. Pass screen initialization using the |data|
  // parameter.
  void ShowScreenWithData(OobeScreenId screen,
                          const base::DictionaryValue* data);

  // Returns the OobeUI instance.
  OobeUI* GetOobeUI() const;

  // Returns current visible OOBE screen.
  OobeScreenId GetCurrentScreen() const;

  // Whether page is ready.
  bool page_is_ready() const { return page_is_ready_; }

 private:
  friend class OobeUI;

  // InsertIntoList takes a template parameter pack and expands into the
  // following form:
  //
  //   for (auto arg : args)
  //     stroage->emplace_back(::login::MakeValue(arg).Clone());
  //
  // This cannot be expressed with the current parameter pack expansion rules
  // and the only way to do it is via compile-time recursion.
  template <typename Head, typename... Tail>
  void InsertIntoList(std::vector<base::Value>* storage,
                      const Head& head,
                      const Tail&... tail) {
    storage->emplace_back(::login::MakeValue(head).Clone());
    InsertIntoList(storage, tail...);
  }
  // Base condition for the recursion, when there are no more elements to
  // insert. Does nothing.
  void InsertIntoList(std::vector<base::Value>*);

  // Record |function_name| and |args| as an incoming event if recording is
  // enabled.
  void MaybeRecordIncomingEvent(const std::string& function_name,
                                const base::ListValue* args);

  // These two functions wrap Add(Raw)Callback so that the incoming JavaScript
  // event can be recorded.
  void OnRawCallback(const std::string& function_name,
                     const content::WebUI::MessageCallback callback,
                     const base::ListValue* args);
  template <typename... Args>
  void OnCallback(const std::string& function_name,
                  const base::RepeatingCallback<void(Args...)>& callback,
                  const base::ListValue* args) {
    MaybeRecordIncomingEvent(function_name, args);
    ::login::CallbackWrapper<Args...>(callback, args);
  }

  // Keeps whether page is ready.
  bool page_is_ready_ = false;

  JSCallsContainer* js_calls_container_ = nullptr;  // non-owning pointers.

  DISALLOW_COPY_AND_ASSIGN(BaseWebUIHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_WEBUI_HANDLER_H_
