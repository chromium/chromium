// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/v8_unit_test.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/blink/public/web/blink.h"

namespace {

// |args| are passed through the various JavaScript logging functions such as
// console.log. Returns a string appropriate for logging with LOG(severity).
std::string LogArgs2String(const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::string message;
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    if (first)
      first = false;
    else
      message += " ";

    v8::String::Utf8Value str(args.GetIsolate(), args[i]);
    message += *str;
  }
  return message;
}

// Whether errors were seen.
bool g_had_errors = false;

// testDone results.
bool g_test_result_ok = false;

// Location of test data (currently test/data/webui).
base::FilePath g_test_data_directory;

// Location of generated test data (<(PROGRAM_DIR)/test_data).
base::FilePath g_gen_test_data_directory;

}  // namespace

V8UnitTest::V8UnitTest() : handle_scope_(blink::MainThreadIsolate()) {
  InitPathsAndLibraries();
}

V8UnitTest::~V8UnitTest() {
}

void V8UnitTest::AddLibrary(const base::FilePath& library_path) {
  user_libraries_.push_back(library_path);
}

bool V8UnitTest::ExecuteJavascriptLibraries() {
  std::string utf8_content;
  for (auto user_libraries_iterator = user_libraries_.begin();
       user_libraries_iterator != user_libraries_.end();
       ++user_libraries_iterator) {
    std::string library_content;
    base::FilePath library_file(*user_libraries_iterator);
    if (!user_libraries_iterator->IsAbsolute()) {
      base::FilePath gen_file = g_gen_test_data_directory.Append(library_file);
      library_file = base::PathExists(gen_file) ?
          gen_file : g_test_data_directory.Append(*user_libraries_iterator);
    }
    library_file = base::MakeAbsoluteFilePath(library_file);
    if (!base::ReadFileToString(library_file, &library_content)) {
      ADD_FAILURE() << library_file.value();
      return false;
    }
    ExecuteScriptInContext(library_content, library_file.MaybeAsASCII());
    if (::testing::Test::HasFatalFailure())
      return false;
  }
  return true;
}

bool V8UnitTest::RunJavascriptTestF(const std::string& test_fixture,
                                    const std::string& test_name) {
  g_had_errors = false;
  g_test_result_ok = false;
  std::string test_js;
  if (!ExecuteJavascriptLibraries())
    return false;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Value> function_property = context->Global()->Get(
      v8::String::NewFromUtf8(isolate, "runTest",
                              v8::NewStringType::kInternalized)
          .ToLocalChecked());
  EXPECT_FALSE(function_property.IsEmpty());
  if (::testing::Test::HasNonfatalFailure())
    return false;
  EXPECT_TRUE(function_property->IsFunction());
  if (::testing::Test::HasNonfatalFailure())
    return false;
  v8::Local<v8::Function> function =
      v8::Local<v8::Function>::Cast(function_property);

  v8::Local<v8::Array> params = v8::Array::New(isolate);
  params->Set(0, v8::String::NewFromUtf8(isolate, test_fixture.data(),
                                         v8::NewStringType::kNormal,
                                         test_fixture.size())
                     .ToLocalChecked());
  params->Set(
      1, v8::String::NewFromUtf8(isolate, test_name.data(),
                                 v8::NewStringType::kNormal, test_name.size())
             .ToLocalChecked());
  v8::Local<v8::Value> args[] = {
      v8::Boolean::New(isolate, false),
      v8::String::NewFromUtf8(isolate, "RUN_TEST_F", v8::NewStringType::kNormal)
          .ToLocalChecked(),
      params};

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> result = function->Call(context->Global(), 3, args);
  // The test fails if an exception was thrown.
  EXPECT_FALSE(result.IsEmpty());
  if (::testing::Test::HasNonfatalFailure())
    return false;

  // Ok if ran successfully, passed tests, and didn't have console errors.
  return result->BooleanValue(context).ToChecked() && g_test_result_ok &&
         !g_had_errors;
}

void V8UnitTest::InitPathsAndLibraries() {
  base::FilePath test_data;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));

  g_test_data_directory = test_data.AppendASCII("webui");

  ASSERT_TRUE(base::PathService::Get(chrome::DIR_GEN_TEST_DATA,
                                     &g_gen_test_data_directory));

  base::FilePath src_root;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root));

  AddLibrary(src_root.AppendASCII("chrome")
                     .AppendASCII("third_party")
                     .AppendASCII("mock4js")
                     .AppendASCII("mock4js.js"));

  AddLibrary(src_root.AppendASCII("third_party")
                     .AppendASCII("chaijs")
                     .AppendASCII("chai.js"));

  AddLibrary(src_root.AppendASCII("third_party")
                     .AppendASCII("accessibility-audit")
                     .AppendASCII("axs_testing.js"));

  AddLibrary(g_test_data_directory.AppendASCII("test_api.js"));
}

void V8UnitTest::SetUp() {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  v8::Local<v8::String> log_string =
      v8::String::NewFromUtf8(isolate, "log", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  v8::Local<v8::FunctionTemplate> log_function =
      v8::FunctionTemplate::New(isolate, &V8UnitTest::Log);
  log_function->RemovePrototype();
  global->Set(log_string, log_function);

  // Set up chrome object for chrome.send().
  v8::Local<v8::ObjectTemplate> chrome = v8::ObjectTemplate::New(isolate);
  global->Set(v8::String::NewFromUtf8(isolate, "chrome",
                                      v8::NewStringType::kInternalized)
                  .ToLocalChecked(),
              chrome);
  v8::Local<v8::FunctionTemplate> send_function =
      v8::FunctionTemplate::New(isolate, &V8UnitTest::ChromeSend);
  send_function->RemovePrototype();
  chrome->Set(
      v8::String::NewFromUtf8(isolate, "send", v8::NewStringType::kInternalized)
          .ToLocalChecked(),
      send_function);

  context_.Reset(isolate, v8::Context::New(isolate, NULL, global));

  // Set up console object for console.log(), etc.
  v8::Local<v8::ObjectTemplate> console = v8::ObjectTemplate::New(isolate);
  global->Set(v8::String::NewFromUtf8(isolate, "console",
                                      v8::NewStringType::kInternalized)
                  .ToLocalChecked(),
              console);
  console->Set(log_string, log_function);
  console->Set(
      v8::String::NewFromUtf8(isolate, "info", v8::NewStringType::kInternalized)
          .ToLocalChecked(),
      log_function);
  console->Set(
      v8::String::NewFromUtf8(isolate, "warn", v8::NewStringType::kInternalized)
          .ToLocalChecked(),
      log_function);
  v8::Local<v8::FunctionTemplate> error_function =
      v8::FunctionTemplate::New(isolate, &V8UnitTest::Error);
  error_function->RemovePrototype();
  console->Set(v8::String::NewFromUtf8(isolate, "error",
                                       v8::NewStringType::kInternalized)
                   .ToLocalChecked(),
               error_function);
  {
    v8::Local<v8::Context> context = context_.Get(isolate);
    v8::Context::Scope context_scope(context);
    context->Global()
        ->Set(context,
              v8::String::NewFromUtf8(isolate, "console",
                                      v8::NewStringType::kInternalized)
                  .ToLocalChecked(),
              console->NewInstance(context).ToLocalChecked())
        .ToChecked();
  }
}

void V8UnitTest::SetGlobalStringVar(const std::string& var_name,
                                    const std::string& value) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  context->Global()->Set(
      v8::String::NewFromUtf8(isolate, var_name.c_str(),
                              v8::NewStringType::kInternalized,
                              var_name.length())
          .ToLocalChecked(),
      v8::String::NewFromUtf8(isolate, value.c_str(),
                              v8::NewStringType::kNormal, value.length())
          .ToLocalChecked());
}

void V8UnitTest::ExecuteScriptInContext(const base::StringPiece& script_source,
                                        const base::StringPiece& script_name) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate, script_source.data(),
                              v8::NewStringType::kNormal, script_source.size())
          .ToLocalChecked();
  v8::Local<v8::String> name =
      v8::String::NewFromUtf8(isolate, script_name.data(),
                              v8::NewStringType::kNormal, script_name.size())
          .ToLocalChecked();

  v8::TryCatch try_catch(isolate);
  v8::ScriptOrigin origin(name);
  v8::Local<v8::Script> script;
  // Ensure the script compiled without errors.
  if (!v8::Script::Compile(context, source, &origin).ToLocal(&script))
    FAIL() << ExceptionToString(try_catch);

  // Ensure the script ran without errors.
  if (script->Run(context).IsEmpty())
    FAIL() << ExceptionToString(try_catch);
}

std::string V8UnitTest::ExceptionToString(const v8::TryCatch& try_catch) {
  std::string str;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::String::Utf8Value exception(isolate, try_catch.Exception());
  v8::Local<v8::Message> message(try_catch.Message());
  if (message.IsEmpty()) {
    str.append(base::StringPrintf("%s\n", *exception));
  } else {
    v8::String::Utf8Value filename(isolate,
                                   message->GetScriptOrigin().ResourceName());
    int linenum = message->GetLineNumber(context).ToChecked();
    int colnum = message->GetStartColumn(context).ToChecked();
    str.append(base::StringPrintf(
        "%s:%i:%i %s\n", *filename, linenum, colnum, *exception));
    v8::String::Utf8Value sourceline(
        isolate, message->GetSourceLine(context).ToLocalChecked());
    str.append(base::StringPrintf("%s\n", *sourceline));
  }
  return str;
}

void V8UnitTest::TestFunction(const std::string& function_name) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Value> function_property = context->Global()->Get(
      v8::String::NewFromUtf8(isolate, function_name.c_str(),
                              v8::NewStringType::kInternalized)
          .ToLocalChecked());
  ASSERT_FALSE(function_property.IsEmpty());
  ASSERT_TRUE(function_property->IsFunction());
  v8::Local<v8::Function> function =
      v8::Local<v8::Function>::Cast(function_property);

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> result = function->Call(context->Global(), 0, NULL);
  // The test fails if an exception was thrown.
  if (result.IsEmpty())
    FAIL() << ExceptionToString(try_catch);
}

// static
void V8UnitTest::Log(const v8::FunctionCallbackInfo<v8::Value>& args) {
  LOG(INFO) << LogArgs2String(args);
}

void V8UnitTest::Error(const v8::FunctionCallbackInfo<v8::Value>& args) {
  g_had_errors = true;
  LOG(ERROR) << LogArgs2String(args);
}

void V8UnitTest::ChromeSend(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  // We expect to receive 2 args: ("testResult", [ok, message]). However,
  // chrome.send may pass only one. Therefore we need to ensure we have at least
  // 1, then ensure that the first is "testResult" before checking again for 2.
  EXPECT_LE(1, args.Length());
  if (::testing::Test::HasNonfatalFailure())
    return;
  v8::String::Utf8Value message(isolate, args[0]);
  EXPECT_EQ("testResult", std::string(*message, message.length()));
  if (::testing::Test::HasNonfatalFailure())
    return;
  EXPECT_EQ(2, args.Length());
  if (::testing::Test::HasNonfatalFailure())
    return;
  v8::Local<v8::Array> test_result(args[1].As<v8::Array>());
  EXPECT_EQ(2U, test_result->Length());
  if (::testing::Test::HasNonfatalFailure())
    return;
  g_test_result_ok = test_result->Get(0)
                         ->BooleanValue(isolate->GetCurrentContext())
                         .ToChecked();
  if (!g_test_result_ok) {
    v8::String::Utf8Value message(isolate, test_result->Get(1));
    LOG(ERROR) << *message;
  }
}
