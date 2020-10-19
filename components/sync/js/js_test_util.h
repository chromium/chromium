// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_JS_JS_TEST_UTIL_H_
#define COMPONENTS_SYNC_JS_JS_TEST_UTIL_H_

#include <ostream>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/js/js_backend.h"
#include "components/sync/js/js_controller.h"
#include "components/sync/js/js_event_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class DictionaryValue;
}

namespace syncer {

class JsEventDetails;

// Defined for googletest.  Equivalent to "*os << args.ToString()".
void PrintTo(const JsEventDetails& details, ::std::ostream* os);

// A gmock matcher for JsEventDetails.  Use like:
//
//   EXPECT_CALL(mock, HandleJsEvent("foo", HasArgs(expected_details)));
::testing::Matcher<const JsEventDetails&> HasDetails(
    const JsEventDetails& expected_details);

// Like HasDetails() but takes a DictionaryValue instead.
::testing::Matcher<const JsEventDetails&> HasDetailsAsDictionary(
    const base::DictionaryValue& expected_details);

// Mocks.

class MockJsBackend : public JsBackend,
                      public base::SupportsWeakPtr<MockJsBackend> {
 public:
  MockJsBackend();
  ~MockJsBackend() override;

  WeakHandle<JsBackend> AsWeakHandle();
  MOCK_METHOD(void,
              SetJsEventHandler,
              (const WeakHandle<JsEventHandler>&),
              (override));
};

class MockJsController : public JsController,
                         public base::SupportsWeakPtr<MockJsController> {
 public:
  MockJsController();
  ~MockJsController() override;
  MOCK_METHOD(void, AddJsEventHandler, (JsEventHandler*), (override));
  MOCK_METHOD(void, RemoveJsEventHandler, (JsEventHandler*), (override));
};

class MockJsEventHandler : public JsEventHandler,
                           public base::SupportsWeakPtr<MockJsEventHandler> {
 public:
  MockJsEventHandler();
  ~MockJsEventHandler() override;

  WeakHandle<JsEventHandler> AsWeakHandle();
  MOCK_METHOD(void,
              HandleJsEvent,
              (const ::std::string&, const JsEventDetails&),
              (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_JS_JS_TEST_UTIL_H_
