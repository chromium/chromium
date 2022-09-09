// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppp_instance.h"

namespace {


// PostMessage is complicated in this test.  We're setting up an iframe with
// src=foo where foo is handled by an extension as a content handler.  Webkit
// generates the html page with the plugin embed, so we have no way to place
// an event handler on the plugin or body of that page that's guaranteed to
// execute before the plugin initializes.  Instead, we're just caching the
// first error (if any) we encounter during load and responding to a normal
// test message to return the results.
const int kDocLoadErrorNone = 0;
const int kDocLoadErrorInit = -1;
int error_in_doc_load_line = kDocLoadErrorInit;

#define EXPECT_ON_LOAD(condition) \
  do { \
    if (!(condition)) { \
      if (kDocLoadErrorInit == error_in_doc_load_line) { \
        error_in_doc_load_line = __LINE__; \
      } \
    } \
  } while (0);

#define ON_LOAD_PASSED \
  do { \
    if (kDocLoadErrorInit == error_in_doc_load_line) \
      error_in_doc_load_line = kDocLoadErrorNone; \
  } while (0);

// Simple 1K buffer to hold the document passed through HandleDocumentLoad.
const uint32_t kMaxFileSize = 1024;
char buffer[kMaxFileSize];
uint32_t buffer_pos = 0;

const char kKnownFileContents[] =
    "This is just a test file so we can verify HandleDocumentLoad.";

void ReadCallback(void* user_data, int32_t pp_error_or_bytes) {
  PP_Resource url_loader = reinterpret_cast<PP_Resource>(user_data);

  EXPECT_ON_LOAD(pp_error_or_bytes >= PP_OK);
  if (pp_error_or_bytes < PP_OK) {
    PPBCore()->ReleaseResource(url_loader);
    return;
  }

  if (PP_OK == pp_error_or_bytes) {
    // Check the contents of the file against the known contents.
    int diff = strncmp(buffer,
                      kKnownFileContents,
                      strlen(kKnownFileContents));
    EXPECT_ON_LOAD(diff == 0);
    PPBURLLoader()->Close(url_loader);
    PPBCore()->ReleaseResource(url_loader);
    ON_LOAD_PASSED;
  } else {
    buffer_pos += pp_error_or_bytes;
    PP_CompletionCallback callback =
        PP_MakeCompletionCallback(ReadCallback, user_data);
    pp_error_or_bytes =
        PPBURLLoader()->ReadResponseBody(url_loader,
                                         buffer + buffer_pos,
                                         kMaxFileSize - buffer_pos,
                                         callback);
    EXPECT(pp_error_or_bytes == PP_OK_COMPLETIONPENDING);
  }
}

PP_Bool HandleDocumentLoad(PP_Instance instance,
                           PP_Resource url_loader) {
  // The browser will release url_loader after this method returns. We need to
  // keep it around until we have read all bytes or get an error.
  PPBCore()->AddRefResource(url_loader);
  void* user_data = reinterpret_cast<void*>(url_loader);
  PP_CompletionCallback callback =
      PP_MakeCompletionCallback(ReadCallback, user_data);
  int32_t pp_error_or_bytes = PPBURLLoader()->ReadResponseBody(url_loader,
                                                               buffer,
                                                               kMaxFileSize,
                                                               callback);
  EXPECT(pp_error_or_bytes == PP_OK_COMPLETIONPENDING);
  return PP_TRUE;
}

const PPP_Instance ppp_instance_interface = {
  DidCreateDefault,
  DidDestroyDefault,
  DidChangeViewDefault,
  DidChangeFocusDefault,
  HandleDocumentLoad
};

// This tests PPP_Instance::HandleDocumentLoad.
void TestHandleDocumentLoad() {
  if (error_in_doc_load_line != kDocLoadErrorNone) {
    char error[1024];
    snprintf(error, sizeof(error),
        "ERROR at %s:%d: Document Load Failed\n",
        __FILE__, error_in_doc_load_line);
    fprintf(stderr, "%s", error);
    PostTestMessage(__FUNCTION__, error);
  }
  TEST_PASSED;
}

// This tests PPB_Instance::IsFullFrame when the plugin is full frame.
// Other conditions for IsFullFrame are tested by ppb_instance tests.
void TestIsFullFrame() {
  PP_Bool full_frame = PPBInstance()->IsFullFrame(pp_instance());
  EXPECT(full_frame == PP_TRUE);
  TEST_PASSED;
}


}  // namespace


void SetupTests() {
  RegisterTest("TestHandleDocumentLoad", TestHandleDocumentLoad);
  RegisterTest("TestIsFullFrame", TestIsFullFrame);
}

void SetupPluginInterfaces() {
  RegisterPluginInterface(PPP_INSTANCE_INTERFACE, &ppp_instance_interface);
}
