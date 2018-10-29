// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_bindings.h"

#include <memory>

#include "base/bind.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8.h"

namespace extensions {

AppBindings::AppBindings(Dispatcher* dispatcher, ScriptContext* context)
    : ObjectBackedNativeHandler(context), app_core_(dispatcher) {}

AppBindings::~AppBindings() {}

void AppBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetIsInstalled", "app.getIsInstalled",
      base::Bind(&AppBindings::GetIsInstalled, base::Unretained(this)));
  RouteHandlerFunction(
      "GetDetails", "app.getDetails",
      base::Bind(&AppBindings::GetDetails, base::Unretained(this)));
  RouteHandlerFunction(
      "GetInstallState", "app.installState",
      base::Bind(&AppBindings::GetInstallState, base::Unretained(this)));
  RouteHandlerFunction(
      "GetRunningState", "app.runningState",
      base::Bind(&AppBindings::GetRunningState, base::Unretained(this)));
}

void AppBindings::GetIsInstalled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(app_core_.GetIsInstalled(context()));
}

void AppBindings::GetDetails(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(app_core_.GetDetails(context()));
}

void AppBindings::GetInstallState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());
  int callback_id = args[0].As<v8::Int32>()->Value();

  app_core_.GetInstallState(
      context(), base::BindOnce(&AppBindings::OnAppInstallStateResponse,
                                base::Unretained(this), callback_id));
}

void AppBindings::GetRunningState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(context()->isolate(),
                              app_core_.GetRunningState(context()),
                              v8::NewStringType::kInternalized)
          .ToLocalChecked());
}

void AppBindings::OnAppInstallStateResponse(int callback_id,
                                            const std::string& state) {
  if (!is_valid())
    return;

  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Value> argv[] = {
      v8::String::NewFromUtf8(isolate, state.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      v8::Integer::New(isolate, callback_id)};
  context()->module_system()->CallModuleMethodSafe(
      "app", "onInstallStateResponse", arraysize(argv), argv);
}

}  // namespace extensions
