// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#include "components/cronet/native/generated/cronet.idl_c.h"

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test of Cronet_Buffer interface.
class Cronet_BufferTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_BufferTest() = default;
  ~Cronet_BufferTest() override = default;

 public:
  bool InitWithDataAndCallback_called_ = false;
  bool InitWithAlloc_called_ = false;
  bool GetSize_called_ = false;
  bool GetData_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_BufferTest);
};

namespace {
// Implementation of Cronet_Buffer methods for testing.
void TestCronet_Buffer_InitWithDataAndCallback(
    Cronet_BufferPtr self,
    Cronet_RawDataPtr data,
    uint64_t size,
    Cronet_BufferCallbackPtr callback) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Buffer_GetClientContext(self);
  auto* test = static_cast<Cronet_BufferTest*>(client_context);
  CHECK(test);
  test->InitWithDataAndCallback_called_ = true;
}
void TestCronet_Buffer_InitWithAlloc(Cronet_BufferPtr self, uint64_t size) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Buffer_GetClientContext(self);
  auto* test = static_cast<Cronet_BufferTest*>(client_context);
  CHECK(test);
  test->InitWithAlloc_called_ = true;
}
uint64_t TestCronet_Buffer_GetSize(Cronet_BufferPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Buffer_GetClientContext(self);
  auto* test = static_cast<Cronet_BufferTest*>(client_context);
  CHECK(test);
  test->GetSize_called_ = true;

  return static_cast<uint64_t>(0);
}
Cronet_RawDataPtr TestCronet_Buffer_GetData(Cronet_BufferPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Buffer_GetClientContext(self);
  auto* test = static_cast<Cronet_BufferTest*>(client_context);
  CHECK(test);
  test->GetData_called_ = true;

  return static_cast<Cronet_RawDataPtr>(0);
}
}  // namespace

// Test that Cronet_Buffer stub forwards function calls as expected.
TEST_F(Cronet_BufferTest, TestCreate) {
  Cronet_BufferPtr test = Cronet_Buffer_CreateWith(
      TestCronet_Buffer_InitWithDataAndCallback,
      TestCronet_Buffer_InitWithAlloc, TestCronet_Buffer_GetSize,
      TestCronet_Buffer_GetData);
  CHECK(test);
  Cronet_Buffer_SetClientContext(test, this);
  CHECK(!InitWithDataAndCallback_called_);
  CHECK(!InitWithAlloc_called_);
  Cronet_Buffer_GetSize(test);
  CHECK(GetSize_called_);
  Cronet_Buffer_GetData(test);
  CHECK(GetData_called_);

  Cronet_Buffer_Destroy(test);
}

// Test of Cronet_BufferCallback interface.
class Cronet_BufferCallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_BufferCallbackTest() = default;
  ~Cronet_BufferCallbackTest() override = default;

 public:
  bool OnDestroy_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_BufferCallbackTest);
};

namespace {
// Implementation of Cronet_BufferCallback methods for testing.
void TestCronet_BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                         Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_BufferCallback_GetClientContext(self);
  auto* test = static_cast<Cronet_BufferCallbackTest*>(client_context);
  CHECK(test);
  test->OnDestroy_called_ = true;
}
}  // namespace

// Test that Cronet_BufferCallback stub forwards function calls as expected.
TEST_F(Cronet_BufferCallbackTest, TestCreate) {
  Cronet_BufferCallbackPtr test =
      Cronet_BufferCallback_CreateWith(TestCronet_BufferCallback_OnDestroy);
  CHECK(test);
  Cronet_BufferCallback_SetClientContext(test, this);
  CHECK(!OnDestroy_called_);

  Cronet_BufferCallback_Destroy(test);
}

// Test of Cronet_Runnable interface.
class Cronet_RunnableTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_RunnableTest() = default;
  ~Cronet_RunnableTest() override = default;

 public:
  bool Run_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_RunnableTest);
};

namespace {
// Implementation of Cronet_Runnable methods for testing.
void TestCronet_Runnable_Run(Cronet_RunnablePtr self) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Runnable_GetClientContext(self);
  auto* test = static_cast<Cronet_RunnableTest*>(client_context);
  CHECK(test);
  test->Run_called_ = true;
}
}  // namespace

// Test that Cronet_Runnable stub forwards function calls as expected.
TEST_F(Cronet_RunnableTest, TestCreate) {
  Cronet_RunnablePtr test = Cronet_Runnable_CreateWith(TestCronet_Runnable_Run);
  CHECK(test);
  Cronet_Runnable_SetClientContext(test, this);
  Cronet_Runnable_Run(test);
  CHECK(Run_called_);

  Cronet_Runnable_Destroy(test);
}

// Test of Cronet_Executor interface.
class Cronet_ExecutorTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_ExecutorTest() = default;
  ~Cronet_ExecutorTest() override = default;

 public:
  bool Execute_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_ExecutorTest);
};

namespace {
// Implementation of Cronet_Executor methods for testing.
void TestCronet_Executor_Execute(Cronet_ExecutorPtr self,
                                 Cronet_RunnablePtr command) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Executor_GetClientContext(self);
  auto* test = static_cast<Cronet_ExecutorTest*>(client_context);
  CHECK(test);
  test->Execute_called_ = true;
}
}  // namespace

// Test that Cronet_Executor stub forwards function calls as expected.
TEST_F(Cronet_ExecutorTest, TestCreate) {
  Cronet_ExecutorPtr test =
      Cronet_Executor_CreateWith(TestCronet_Executor_Execute);
  CHECK(test);
  Cronet_Executor_SetClientContext(test, this);
  CHECK(!Execute_called_);

  Cronet_Executor_Destroy(test);
}

// Test of Cronet_Engine interface.
class Cronet_EngineTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_EngineTest() = default;
  ~Cronet_EngineTest() override = default;

 public:
  bool StartWithParams_called_ = false;
  bool StartNetLogToFile_called_ = false;
  bool StopNetLog_called_ = false;
  bool Shutdown_called_ = false;
  bool GetVersionString_called_ = false;
  bool GetDefaultUserAgent_called_ = false;
  bool AddRequestFinishedListener_called_ = false;
  bool RemoveRequestFinishedListener_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_EngineTest);
};

namespace {
// Implementation of Cronet_Engine methods for testing.
Cronet_RESULT TestCronet_Engine_StartWithParams(Cronet_EnginePtr self,
                                                Cronet_EngineParamsPtr params) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Engine_GetClientContext(self);
  auto* test = static_cast<Cronet_EngineTest*>(client_context);
  CHECK(test);
  test->StartWithParams_called_ = true;

  return static_cast<Cronet_RESULT>(0);
}
bool TestCronet_Engine_StartNetLogToFile(Cronet_EnginePtr self,
                                         Cronet_String file_name,
                                         bool log_all) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Engine_GetClientContext(self);
  auto* test = static_cast<Cronet_EngineTest*>(client_context);
  CHECK(test);
  test->StartNetLogToFile_called_ = true;

  return static_cast<bool>(0);
}
void TestCronet_Engine_StopNetLog(Cronet_EnginePtr self) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Engine_GetClientContext(self);
  auto* test = static_cast<Cronet_EngineTest*>(client_context);
  CHECK(test);
  test->StopNetLog_called_ = true;
}
Cronet_RESULT TestCronet_Engine_Shutdown(Cronet_EnginePtr self) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Engine_GetClientContext(self);
  auto* test = static_cast<Cronet_EngineTest*>(client_context);
  CHECK(test);
  test->Shutdown_called_ = true;

  return static_cast<Cronet_RESULT>(0);
}
Cronet_String TestCronet_Engine_GetVersionString(Cronet_EnginePtr self) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Engine_GetClientContext(self);
  auto* test = static_cast<Cronet_EngineTest*>(client_context);
  CHECK(test);
  test->GetVersionString_called_ = true;

  return static_cast<Cronet_String>(0);
}
Cronet_String TestCronet_Engine_GetDefaultUserAgent(Cronet_EnginePtr self) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Engine_GetClientContext(self);
  auto* test = static_cast<Cronet_EngineTest*>(client_context);
  CHECK(test);
  test->GetDefaultUserAgent_called_ = true;

  return static_cast<Cronet_String>(0);
}
void TestCronet_Engine_AddRequestFinishedListener(
    Cronet_EnginePtr self,
    Cronet_RequestFinishedInfoListenerPtr listener,
    Cronet_ExecutorPtr executor) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Engine_GetClientContext(self);
  auto* test = static_cast<Cronet_EngineTest*>(client_context);
  CHECK(test);
  test->AddRequestFinishedListener_called_ = true;
}
void TestCronet_Engine_RemoveRequestFinishedListener(
    Cronet_EnginePtr self,
    Cronet_RequestFinishedInfoListenerPtr listener) {
  CHECK(self);
  Cronet_ClientContext client_context = Cronet_Engine_GetClientContext(self);
  auto* test = static_cast<Cronet_EngineTest*>(client_context);
  CHECK(test);
  test->RemoveRequestFinishedListener_called_ = true;
}
}  // namespace

// Test that Cronet_Engine stub forwards function calls as expected.
TEST_F(Cronet_EngineTest, TestCreate) {
  Cronet_EnginePtr test = Cronet_Engine_CreateWith(
      TestCronet_Engine_StartWithParams, TestCronet_Engine_StartNetLogToFile,
      TestCronet_Engine_StopNetLog, TestCronet_Engine_Shutdown,
      TestCronet_Engine_GetVersionString, TestCronet_Engine_GetDefaultUserAgent,
      TestCronet_Engine_AddRequestFinishedListener,
      TestCronet_Engine_RemoveRequestFinishedListener);
  CHECK(test);
  Cronet_Engine_SetClientContext(test, this);
  CHECK(!StartWithParams_called_);
  CHECK(!StartNetLogToFile_called_);
  Cronet_Engine_StopNetLog(test);
  CHECK(StopNetLog_called_);
  Cronet_Engine_Shutdown(test);
  CHECK(Shutdown_called_);
  Cronet_Engine_GetVersionString(test);
  CHECK(GetVersionString_called_);
  Cronet_Engine_GetDefaultUserAgent(test);
  CHECK(GetDefaultUserAgent_called_);
  CHECK(!AddRequestFinishedListener_called_);
  CHECK(!RemoveRequestFinishedListener_called_);

  Cronet_Engine_Destroy(test);
}

// Test of Cronet_UrlRequestStatusListener interface.
class Cronet_UrlRequestStatusListenerTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UrlRequestStatusListenerTest() = default;
  ~Cronet_UrlRequestStatusListenerTest() override = default;

 public:
  bool OnStatus_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestStatusListenerTest);
};

namespace {
// Implementation of Cronet_UrlRequestStatusListener methods for testing.
void TestCronet_UrlRequestStatusListener_OnStatus(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_UrlRequestStatusListener_Status status) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequestStatusListener_GetClientContext(self);
  auto* test =
      static_cast<Cronet_UrlRequestStatusListenerTest*>(client_context);
  CHECK(test);
  test->OnStatus_called_ = true;
}
}  // namespace

// Test that Cronet_UrlRequestStatusListener stub forwards function calls as
// expected.
TEST_F(Cronet_UrlRequestStatusListenerTest, TestCreate) {
  Cronet_UrlRequestStatusListenerPtr test =
      Cronet_UrlRequestStatusListener_CreateWith(
          TestCronet_UrlRequestStatusListener_OnStatus);
  CHECK(test);
  Cronet_UrlRequestStatusListener_SetClientContext(test, this);
  CHECK(!OnStatus_called_);

  Cronet_UrlRequestStatusListener_Destroy(test);
}

// Test of Cronet_UrlRequestCallback interface.
class Cronet_UrlRequestCallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UrlRequestCallbackTest() = default;
  ~Cronet_UrlRequestCallbackTest() override = default;

 public:
  bool OnRedirectReceived_called_ = false;
  bool OnResponseStarted_called_ = false;
  bool OnReadCompleted_called_ = false;
  bool OnSucceeded_called_ = false;
  bool OnFailed_called_ = false;
  bool OnCanceled_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestCallbackTest);
};

namespace {
// Implementation of Cronet_UrlRequestCallback methods for testing.
void TestCronet_UrlRequestCallback_OnRedirectReceived(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_String new_location_url) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequestCallback_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestCallbackTest*>(client_context);
  CHECK(test);
  test->OnRedirectReceived_called_ = true;
}
void TestCronet_UrlRequestCallback_OnResponseStarted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequestCallback_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestCallbackTest*>(client_context);
  CHECK(test);
  test->OnResponseStarted_called_ = true;
}
void TestCronet_UrlRequestCallback_OnReadCompleted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_BufferPtr buffer,
    uint64_t bytes_read) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequestCallback_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestCallbackTest*>(client_context);
  CHECK(test);
  test->OnReadCompleted_called_ = true;
}
void TestCronet_UrlRequestCallback_OnSucceeded(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequestCallback_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestCallbackTest*>(client_context);
  CHECK(test);
  test->OnSucceeded_called_ = true;
}
void TestCronet_UrlRequestCallback_OnFailed(Cronet_UrlRequestCallbackPtr self,
                                            Cronet_UrlRequestPtr request,
                                            Cronet_UrlResponseInfoPtr info,
                                            Cronet_ErrorPtr error) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequestCallback_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestCallbackTest*>(client_context);
  CHECK(test);
  test->OnFailed_called_ = true;
}
void TestCronet_UrlRequestCallback_OnCanceled(Cronet_UrlRequestCallbackPtr self,
                                              Cronet_UrlRequestPtr request,
                                              Cronet_UrlResponseInfoPtr info) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequestCallback_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestCallbackTest*>(client_context);
  CHECK(test);
  test->OnCanceled_called_ = true;
}
}  // namespace

// Test that Cronet_UrlRequestCallback stub forwards function calls as expected.
TEST_F(Cronet_UrlRequestCallbackTest, TestCreate) {
  Cronet_UrlRequestCallbackPtr test = Cronet_UrlRequestCallback_CreateWith(
      TestCronet_UrlRequestCallback_OnRedirectReceived,
      TestCronet_UrlRequestCallback_OnResponseStarted,
      TestCronet_UrlRequestCallback_OnReadCompleted,
      TestCronet_UrlRequestCallback_OnSucceeded,
      TestCronet_UrlRequestCallback_OnFailed,
      TestCronet_UrlRequestCallback_OnCanceled);
  CHECK(test);
  Cronet_UrlRequestCallback_SetClientContext(test, this);
  CHECK(!OnRedirectReceived_called_);
  CHECK(!OnResponseStarted_called_);
  CHECK(!OnReadCompleted_called_);
  CHECK(!OnSucceeded_called_);
  CHECK(!OnFailed_called_);
  CHECK(!OnCanceled_called_);

  Cronet_UrlRequestCallback_Destroy(test);
}

// Test of Cronet_UploadDataSink interface.
class Cronet_UploadDataSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UploadDataSinkTest() = default;
  ~Cronet_UploadDataSinkTest() override = default;

 public:
  bool OnReadSucceeded_called_ = false;
  bool OnReadError_called_ = false;
  bool OnRewindSucceeded_called_ = false;
  bool OnRewindError_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataSinkTest);
};

namespace {
// Implementation of Cronet_UploadDataSink methods for testing.
void TestCronet_UploadDataSink_OnReadSucceeded(Cronet_UploadDataSinkPtr self,
                                               uint64_t bytes_read,
                                               bool final_chunk) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UploadDataSink_GetClientContext(self);
  auto* test = static_cast<Cronet_UploadDataSinkTest*>(client_context);
  CHECK(test);
  test->OnReadSucceeded_called_ = true;
}
void TestCronet_UploadDataSink_OnReadError(Cronet_UploadDataSinkPtr self,
                                           Cronet_String error_message) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UploadDataSink_GetClientContext(self);
  auto* test = static_cast<Cronet_UploadDataSinkTest*>(client_context);
  CHECK(test);
  test->OnReadError_called_ = true;
}
void TestCronet_UploadDataSink_OnRewindSucceeded(
    Cronet_UploadDataSinkPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UploadDataSink_GetClientContext(self);
  auto* test = static_cast<Cronet_UploadDataSinkTest*>(client_context);
  CHECK(test);
  test->OnRewindSucceeded_called_ = true;
}
void TestCronet_UploadDataSink_OnRewindError(Cronet_UploadDataSinkPtr self,
                                             Cronet_String error_message) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UploadDataSink_GetClientContext(self);
  auto* test = static_cast<Cronet_UploadDataSinkTest*>(client_context);
  CHECK(test);
  test->OnRewindError_called_ = true;
}
}  // namespace

// Test that Cronet_UploadDataSink stub forwards function calls as expected.
TEST_F(Cronet_UploadDataSinkTest, TestCreate) {
  Cronet_UploadDataSinkPtr test = Cronet_UploadDataSink_CreateWith(
      TestCronet_UploadDataSink_OnReadSucceeded,
      TestCronet_UploadDataSink_OnReadError,
      TestCronet_UploadDataSink_OnRewindSucceeded,
      TestCronet_UploadDataSink_OnRewindError);
  CHECK(test);
  Cronet_UploadDataSink_SetClientContext(test, this);
  CHECK(!OnReadSucceeded_called_);
  CHECK(!OnReadError_called_);
  Cronet_UploadDataSink_OnRewindSucceeded(test);
  CHECK(OnRewindSucceeded_called_);
  CHECK(!OnRewindError_called_);

  Cronet_UploadDataSink_Destroy(test);
}

// Test of Cronet_UploadDataProvider interface.
class Cronet_UploadDataProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UploadDataProviderTest() = default;
  ~Cronet_UploadDataProviderTest() override = default;

 public:
  bool GetLength_called_ = false;
  bool Read_called_ = false;
  bool Rewind_called_ = false;
  bool Close_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataProviderTest);
};

namespace {
// Implementation of Cronet_UploadDataProvider methods for testing.
int64_t TestCronet_UploadDataProvider_GetLength(
    Cronet_UploadDataProviderPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UploadDataProvider_GetClientContext(self);
  auto* test = static_cast<Cronet_UploadDataProviderTest*>(client_context);
  CHECK(test);
  test->GetLength_called_ = true;

  return static_cast<int64_t>(0);
}
void TestCronet_UploadDataProvider_Read(
    Cronet_UploadDataProviderPtr self,
    Cronet_UploadDataSinkPtr upload_data_sink,
    Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UploadDataProvider_GetClientContext(self);
  auto* test = static_cast<Cronet_UploadDataProviderTest*>(client_context);
  CHECK(test);
  test->Read_called_ = true;
}
void TestCronet_UploadDataProvider_Rewind(
    Cronet_UploadDataProviderPtr self,
    Cronet_UploadDataSinkPtr upload_data_sink) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UploadDataProvider_GetClientContext(self);
  auto* test = static_cast<Cronet_UploadDataProviderTest*>(client_context);
  CHECK(test);
  test->Rewind_called_ = true;
}
void TestCronet_UploadDataProvider_Close(Cronet_UploadDataProviderPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UploadDataProvider_GetClientContext(self);
  auto* test = static_cast<Cronet_UploadDataProviderTest*>(client_context);
  CHECK(test);
  test->Close_called_ = true;
}
}  // namespace

// Test that Cronet_UploadDataProvider stub forwards function calls as expected.
TEST_F(Cronet_UploadDataProviderTest, TestCreate) {
  Cronet_UploadDataProviderPtr test = Cronet_UploadDataProvider_CreateWith(
      TestCronet_UploadDataProvider_GetLength,
      TestCronet_UploadDataProvider_Read, TestCronet_UploadDataProvider_Rewind,
      TestCronet_UploadDataProvider_Close);
  CHECK(test);
  Cronet_UploadDataProvider_SetClientContext(test, this);
  Cronet_UploadDataProvider_GetLength(test);
  CHECK(GetLength_called_);
  CHECK(!Read_called_);
  CHECK(!Rewind_called_);
  Cronet_UploadDataProvider_Close(test);
  CHECK(Close_called_);

  Cronet_UploadDataProvider_Destroy(test);
}

// Test of Cronet_UrlRequest interface.
class Cronet_UrlRequestTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UrlRequestTest() = default;
  ~Cronet_UrlRequestTest() override = default;

 public:
  bool InitWithParams_called_ = false;
  bool Start_called_ = false;
  bool FollowRedirect_called_ = false;
  bool Read_called_ = false;
  bool Cancel_called_ = false;
  bool IsDone_called_ = false;
  bool GetStatus_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestTest);
};

namespace {
// Implementation of Cronet_UrlRequest methods for testing.
Cronet_RESULT TestCronet_UrlRequest_InitWithParams(
    Cronet_UrlRequestPtr self,
    Cronet_EnginePtr engine,
    Cronet_String url,
    Cronet_UrlRequestParamsPtr params,
    Cronet_UrlRequestCallbackPtr callback,
    Cronet_ExecutorPtr executor) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequest_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestTest*>(client_context);
  CHECK(test);
  test->InitWithParams_called_ = true;

  return static_cast<Cronet_RESULT>(0);
}
Cronet_RESULT TestCronet_UrlRequest_Start(Cronet_UrlRequestPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequest_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestTest*>(client_context);
  CHECK(test);
  test->Start_called_ = true;

  return static_cast<Cronet_RESULT>(0);
}
Cronet_RESULT TestCronet_UrlRequest_FollowRedirect(Cronet_UrlRequestPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequest_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestTest*>(client_context);
  CHECK(test);
  test->FollowRedirect_called_ = true;

  return static_cast<Cronet_RESULT>(0);
}
Cronet_RESULT TestCronet_UrlRequest_Read(Cronet_UrlRequestPtr self,
                                         Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequest_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestTest*>(client_context);
  CHECK(test);
  test->Read_called_ = true;

  return static_cast<Cronet_RESULT>(0);
}
void TestCronet_UrlRequest_Cancel(Cronet_UrlRequestPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequest_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestTest*>(client_context);
  CHECK(test);
  test->Cancel_called_ = true;
}
bool TestCronet_UrlRequest_IsDone(Cronet_UrlRequestPtr self) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequest_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestTest*>(client_context);
  CHECK(test);
  test->IsDone_called_ = true;

  return static_cast<bool>(0);
}
void TestCronet_UrlRequest_GetStatus(
    Cronet_UrlRequestPtr self,
    Cronet_UrlRequestStatusListenerPtr listener) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_UrlRequest_GetClientContext(self);
  auto* test = static_cast<Cronet_UrlRequestTest*>(client_context);
  CHECK(test);
  test->GetStatus_called_ = true;
}
}  // namespace

// Test that Cronet_UrlRequest stub forwards function calls as expected.
TEST_F(Cronet_UrlRequestTest, TestCreate) {
  Cronet_UrlRequestPtr test = Cronet_UrlRequest_CreateWith(
      TestCronet_UrlRequest_InitWithParams, TestCronet_UrlRequest_Start,
      TestCronet_UrlRequest_FollowRedirect, TestCronet_UrlRequest_Read,
      TestCronet_UrlRequest_Cancel, TestCronet_UrlRequest_IsDone,
      TestCronet_UrlRequest_GetStatus);
  CHECK(test);
  Cronet_UrlRequest_SetClientContext(test, this);
  CHECK(!InitWithParams_called_);
  Cronet_UrlRequest_Start(test);
  CHECK(Start_called_);
  Cronet_UrlRequest_FollowRedirect(test);
  CHECK(FollowRedirect_called_);
  CHECK(!Read_called_);
  Cronet_UrlRequest_Cancel(test);
  CHECK(Cancel_called_);
  Cronet_UrlRequest_IsDone(test);
  CHECK(IsDone_called_);
  CHECK(!GetStatus_called_);

  Cronet_UrlRequest_Destroy(test);
}

// Test of Cronet_RequestFinishedInfoListener interface.
class Cronet_RequestFinishedInfoListenerTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_RequestFinishedInfoListenerTest() = default;
  ~Cronet_RequestFinishedInfoListenerTest() override = default;

 public:
  bool OnRequestFinished_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_RequestFinishedInfoListenerTest);
};

namespace {
// Implementation of Cronet_RequestFinishedInfoListener methods for testing.
void TestCronet_RequestFinishedInfoListener_OnRequestFinished(
    Cronet_RequestFinishedInfoListenerPtr self,
    Cronet_RequestFinishedInfoPtr request_info,
    Cronet_UrlResponseInfoPtr response_info,
    Cronet_ErrorPtr error) {
  CHECK(self);
  Cronet_ClientContext client_context =
      Cronet_RequestFinishedInfoListener_GetClientContext(self);
  auto* test =
      static_cast<Cronet_RequestFinishedInfoListenerTest*>(client_context);
  CHECK(test);
  test->OnRequestFinished_called_ = true;
}
}  // namespace

// Test that Cronet_RequestFinishedInfoListener stub forwards function calls as
// expected.
TEST_F(Cronet_RequestFinishedInfoListenerTest, TestCreate) {
  Cronet_RequestFinishedInfoListenerPtr test =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestCronet_RequestFinishedInfoListener_OnRequestFinished);
  CHECK(test);
  Cronet_RequestFinishedInfoListener_SetClientContext(test, this);
  CHECK(!OnRequestFinished_called_);

  Cronet_RequestFinishedInfoListener_Destroy(test);
}
