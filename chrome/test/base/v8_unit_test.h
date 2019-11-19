// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_V8_UNIT_TEST_H_
#define CHROME_TEST_BASE_V8_UNIT_TEST_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

// A superclass for unit tests that involve running JavaScript.  This class
// sets up V8 context and has methods that make it easy to execute scripts in
// this context as well as call functions in the context.
class V8UnitTest : public testing::Test {
 public:
  V8UnitTest();
  ~V8UnitTest() override;

  // Methods from testing::Test.
  void SetUp() override;

 protected:
  // Add a custom helper JS library for your test. If |library_path| is
  // relative, it'll be read as relative to the test data dir.
  void AddLibrary(const base::FilePath& library_path);

  // Runs |test_fixture|.|test_name| using the framework in test_api.js.
  bool RunJavascriptTestF(const std::string& test_fixture,
                          const std::string& test_name);

  // Executes the given |script_source| in the context. The specified
  // |script_name| is used when reporting errors.
  virtual void ExecuteScriptInContext(const base::StringPiece& script_source,
                                      const base::StringPiece& script_name);

  // Set the variable |var_name| to a string |value| in the global scope.
  virtual void SetGlobalStringVar(const std::string& var_name,
                                  const std::string& value);

  // Converts the v8::TryCatch |try_catch| into a human readable string.
  virtual std::string ExceptionToString(const v8::TryCatch& try_catch);

  // Calls the specified |function_name| that resides in the global scope of the
  // context. If the function throws an exception, FAIL() is called to indicate
  // a unit test failure. This is useful for executing unit test functions
  // implemented in JavaScript.
  virtual void TestFunction(const std::string& function_name);

  // This method is bound to a global function "log" in the context, as well as
  // to log, warn, and info of the console object. Scripts running in the
  // context can call this with |args| to print out logging information to the
  // console.
  static void Log(const v8::FunctionCallbackInfo<v8::Value>& args);

  // This method is bound to console.error in the context. Any calls to this
  // will log |args| to the console and also signal an error condition causing
  // |RunJavascriptF| to fail.
  static void Error(const v8::FunctionCallbackInfo<v8::Value>& args);

  // This method is bound to a method "chrome.send" in the context. When
  // test_api calls testDone with |args| to report its results, this will
  // capture and hold the results for analysis by |RunJavascriptF|.
  static void ChromeSend(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  // Executes all added javascript libraries. Returns true if no errors.
  bool ExecuteJavascriptLibraries();

  // Initializes paths and libraries.
  void InitPathsAndLibraries();

  base::test::TaskEnvironment task_environment_;

  // Handle scope that is used throughout the life of this class.
  v8::HandleScope handle_scope_;

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;

  // User added libraries.
  std::vector<base::FilePath> user_libraries_;
};

#endif  // CHROME_TEST_BASE_V8_UNIT_TEST_H_
