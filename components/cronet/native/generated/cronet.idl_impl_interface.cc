// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#include "components/cronet/native/generated/cronet.idl_impl_interface.h"

#include "base/check.h"

// C functions of Cronet_Buffer that forward calls to C++ implementation.
void Cronet_Buffer_Destroy(Cronet_BufferPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_Buffer_SetClientContext(Cronet_BufferPtr self,
                                    Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_Buffer_GetClientContext(Cronet_BufferPtr self) {
  DCHECK(self);
  return self->client_context();
}

void Cronet_Buffer_InitWithDataAndCallback(Cronet_BufferPtr self,
                                           Cronet_RawDataPtr data,
                                           uint64_t size,
                                           Cronet_BufferCallbackPtr callback) {
  DCHECK(self);
  self->InitWithDataAndCallback(data, size, callback);
}

void Cronet_Buffer_InitWithAlloc(Cronet_BufferPtr self, uint64_t size) {
  DCHECK(self);
  self->InitWithAlloc(size);
}

uint64_t Cronet_Buffer_GetSize(Cronet_BufferPtr self) {
  DCHECK(self);
  return self->GetSize();
}

Cronet_RawDataPtr Cronet_Buffer_GetData(Cronet_BufferPtr self) {
  DCHECK(self);
  return self->GetData();
}

// Implementation of Cronet_Buffer that forwards calls to C functions
// implemented by the app.
class Cronet_BufferStub : public Cronet_Buffer {
 public:
  Cronet_BufferStub(
      Cronet_Buffer_InitWithDataAndCallbackFunc InitWithDataAndCallbackFunc,
      Cronet_Buffer_InitWithAllocFunc InitWithAllocFunc,
      Cronet_Buffer_GetSizeFunc GetSizeFunc,
      Cronet_Buffer_GetDataFunc GetDataFunc)
      : InitWithDataAndCallbackFunc_(InitWithDataAndCallbackFunc),
        InitWithAllocFunc_(InitWithAllocFunc),
        GetSizeFunc_(GetSizeFunc),
        GetDataFunc_(GetDataFunc) {}

  Cronet_BufferStub(const Cronet_BufferStub&) = delete;
  Cronet_BufferStub& operator=(const Cronet_BufferStub&) = delete;

  ~Cronet_BufferStub() override {}

 protected:
  void InitWithDataAndCallback(Cronet_RawDataPtr data,
                               uint64_t size,
                               Cronet_BufferCallbackPtr callback) override {
    InitWithDataAndCallbackFunc_(this, data, size, callback);
  }

  void InitWithAlloc(uint64_t size) override { InitWithAllocFunc_(this, size); }

  uint64_t GetSize() override { return GetSizeFunc_(this); }

  Cronet_RawDataPtr GetData() override { return GetDataFunc_(this); }

 private:
  const Cronet_Buffer_InitWithDataAndCallbackFunc InitWithDataAndCallbackFunc_;
  const Cronet_Buffer_InitWithAllocFunc InitWithAllocFunc_;
  const Cronet_Buffer_GetSizeFunc GetSizeFunc_;
  const Cronet_Buffer_GetDataFunc GetDataFunc_;
};

Cronet_BufferPtr Cronet_Buffer_CreateWith(
    Cronet_Buffer_InitWithDataAndCallbackFunc InitWithDataAndCallbackFunc,
    Cronet_Buffer_InitWithAllocFunc InitWithAllocFunc,
    Cronet_Buffer_GetSizeFunc GetSizeFunc,
    Cronet_Buffer_GetDataFunc GetDataFunc) {
  return new Cronet_BufferStub(InitWithDataAndCallbackFunc, InitWithAllocFunc,
                               GetSizeFunc, GetDataFunc);
}

// C functions of Cronet_BufferCallback that forward calls to C++
// implementation.
void Cronet_BufferCallback_Destroy(Cronet_BufferCallbackPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_BufferCallback_SetClientContext(
    Cronet_BufferCallbackPtr self,
    Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_BufferCallback_GetClientContext(
    Cronet_BufferCallbackPtr self) {
  DCHECK(self);
  return self->client_context();
}

void Cronet_BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                     Cronet_BufferPtr buffer) {
  DCHECK(self);
  self->OnDestroy(buffer);
}

// Implementation of Cronet_BufferCallback that forwards calls to C functions
// implemented by the app.
class Cronet_BufferCallbackStub : public Cronet_BufferCallback {
 public:
  explicit Cronet_BufferCallbackStub(
      Cronet_BufferCallback_OnDestroyFunc OnDestroyFunc)
      : OnDestroyFunc_(OnDestroyFunc) {}

  Cronet_BufferCallbackStub(const Cronet_BufferCallbackStub&) = delete;
  Cronet_BufferCallbackStub& operator=(const Cronet_BufferCallbackStub&) =
      delete;

  ~Cronet_BufferCallbackStub() override {}

 protected:
  void OnDestroy(Cronet_BufferPtr buffer) override {
    OnDestroyFunc_(this, buffer);
  }

 private:
  const Cronet_BufferCallback_OnDestroyFunc OnDestroyFunc_;
};

Cronet_BufferCallbackPtr Cronet_BufferCallback_CreateWith(
    Cronet_BufferCallback_OnDestroyFunc OnDestroyFunc) {
  return new Cronet_BufferCallbackStub(OnDestroyFunc);
}

// C functions of Cronet_Runnable that forward calls to C++ implementation.
void Cronet_Runnable_Destroy(Cronet_RunnablePtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_Runnable_SetClientContext(Cronet_RunnablePtr self,
                                      Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_Runnable_GetClientContext(Cronet_RunnablePtr self) {
  DCHECK(self);
  return self->client_context();
}

void Cronet_Runnable_Run(Cronet_RunnablePtr self) {
  DCHECK(self);
  self->Run();
}

// Implementation of Cronet_Runnable that forwards calls to C functions
// implemented by the app.
class Cronet_RunnableStub : public Cronet_Runnable {
 public:
  explicit Cronet_RunnableStub(Cronet_Runnable_RunFunc RunFunc)
      : RunFunc_(RunFunc) {}

  Cronet_RunnableStub(const Cronet_RunnableStub&) = delete;
  Cronet_RunnableStub& operator=(const Cronet_RunnableStub&) = delete;

  ~Cronet_RunnableStub() override {}

 protected:
  void Run() override { RunFunc_(this); }

 private:
  const Cronet_Runnable_RunFunc RunFunc_;
};

Cronet_RunnablePtr Cronet_Runnable_CreateWith(Cronet_Runnable_RunFunc RunFunc) {
  return new Cronet_RunnableStub(RunFunc);
}

// C functions of Cronet_Executor that forward calls to C++ implementation.
void Cronet_Executor_Destroy(Cronet_ExecutorPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_Executor_SetClientContext(Cronet_ExecutorPtr self,
                                      Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_Executor_GetClientContext(Cronet_ExecutorPtr self) {
  DCHECK(self);
  return self->client_context();
}

void Cronet_Executor_Execute(Cronet_ExecutorPtr self,
                             Cronet_RunnablePtr command) {
  DCHECK(self);
  self->Execute(command);
}

// Implementation of Cronet_Executor that forwards calls to C functions
// implemented by the app.
class Cronet_ExecutorStub : public Cronet_Executor {
 public:
  explicit Cronet_ExecutorStub(Cronet_Executor_ExecuteFunc ExecuteFunc)
      : ExecuteFunc_(ExecuteFunc) {}

  Cronet_ExecutorStub(const Cronet_ExecutorStub&) = delete;
  Cronet_ExecutorStub& operator=(const Cronet_ExecutorStub&) = delete;

  ~Cronet_ExecutorStub() override {}

 protected:
  void Execute(Cronet_RunnablePtr command) override {
    ExecuteFunc_(this, command);
  }

 private:
  const Cronet_Executor_ExecuteFunc ExecuteFunc_;
};

Cronet_ExecutorPtr Cronet_Executor_CreateWith(
    Cronet_Executor_ExecuteFunc ExecuteFunc) {
  return new Cronet_ExecutorStub(ExecuteFunc);
}

// C functions of Cronet_Engine that forward calls to C++ implementation.
void Cronet_Engine_Destroy(Cronet_EnginePtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_Engine_SetClientContext(Cronet_EnginePtr self,
                                    Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_Engine_GetClientContext(Cronet_EnginePtr self) {
  DCHECK(self);
  return self->client_context();
}

Cronet_RESULT Cronet_Engine_StartWithParams(Cronet_EnginePtr self,
                                            Cronet_EngineParamsPtr params) {
  DCHECK(self);
  return self->StartWithParams(params);
}

bool Cronet_Engine_StartNetLogToFile(Cronet_EnginePtr self,
                                     Cronet_String file_name,
                                     bool log_all) {
  DCHECK(self);
  return self->StartNetLogToFile(file_name, log_all);
}

void Cronet_Engine_StopNetLog(Cronet_EnginePtr self) {
  DCHECK(self);
  self->StopNetLog();
}

Cronet_RESULT Cronet_Engine_Shutdown(Cronet_EnginePtr self) {
  DCHECK(self);
  return self->Shutdown();
}

Cronet_String Cronet_Engine_GetVersionString(Cronet_EnginePtr self) {
  DCHECK(self);
  return self->GetVersionString();
}

Cronet_String Cronet_Engine_GetDefaultUserAgent(Cronet_EnginePtr self) {
  DCHECK(self);
  return self->GetDefaultUserAgent();
}

void Cronet_Engine_AddRequestFinishedListener(
    Cronet_EnginePtr self,
    Cronet_RequestFinishedInfoListenerPtr listener,
    Cronet_ExecutorPtr executor) {
  DCHECK(self);
  self->AddRequestFinishedListener(listener, executor);
}

void Cronet_Engine_RemoveRequestFinishedListener(
    Cronet_EnginePtr self,
    Cronet_RequestFinishedInfoListenerPtr listener) {
  DCHECK(self);
  self->RemoveRequestFinishedListener(listener);
}

// Implementation of Cronet_Engine that forwards calls to C functions
// implemented by the app.
class Cronet_EngineStub : public Cronet_Engine {
 public:
  Cronet_EngineStub(
      Cronet_Engine_StartWithParamsFunc StartWithParamsFunc,
      Cronet_Engine_StartNetLogToFileFunc StartNetLogToFileFunc,
      Cronet_Engine_StopNetLogFunc StopNetLogFunc,
      Cronet_Engine_ShutdownFunc ShutdownFunc,
      Cronet_Engine_GetVersionStringFunc GetVersionStringFunc,
      Cronet_Engine_GetDefaultUserAgentFunc GetDefaultUserAgentFunc,
      Cronet_Engine_AddRequestFinishedListenerFunc
          AddRequestFinishedListenerFunc,
      Cronet_Engine_RemoveRequestFinishedListenerFunc
          RemoveRequestFinishedListenerFunc)
      : StartWithParamsFunc_(StartWithParamsFunc),
        StartNetLogToFileFunc_(StartNetLogToFileFunc),
        StopNetLogFunc_(StopNetLogFunc),
        ShutdownFunc_(ShutdownFunc),
        GetVersionStringFunc_(GetVersionStringFunc),
        GetDefaultUserAgentFunc_(GetDefaultUserAgentFunc),
        AddRequestFinishedListenerFunc_(AddRequestFinishedListenerFunc),
        RemoveRequestFinishedListenerFunc_(RemoveRequestFinishedListenerFunc) {}

  Cronet_EngineStub(const Cronet_EngineStub&) = delete;
  Cronet_EngineStub& operator=(const Cronet_EngineStub&) = delete;

  ~Cronet_EngineStub() override {}

 protected:
  Cronet_RESULT StartWithParams(Cronet_EngineParamsPtr params) override {
    return StartWithParamsFunc_(this, params);
  }

  bool StartNetLogToFile(Cronet_String file_name, bool log_all) override {
    return StartNetLogToFileFunc_(this, file_name, log_all);
  }

  void StopNetLog() override { StopNetLogFunc_(this); }

  Cronet_RESULT Shutdown() override { return ShutdownFunc_(this); }

  Cronet_String GetVersionString() override {
    return GetVersionStringFunc_(this);
  }

  Cronet_String GetDefaultUserAgent() override {
    return GetDefaultUserAgentFunc_(this);
  }

  void AddRequestFinishedListener(
      Cronet_RequestFinishedInfoListenerPtr listener,
      Cronet_ExecutorPtr executor) override {
    AddRequestFinishedListenerFunc_(this, listener, executor);
  }

  void RemoveRequestFinishedListener(
      Cronet_RequestFinishedInfoListenerPtr listener) override {
    RemoveRequestFinishedListenerFunc_(this, listener);
  }

 private:
  const Cronet_Engine_StartWithParamsFunc StartWithParamsFunc_;
  const Cronet_Engine_StartNetLogToFileFunc StartNetLogToFileFunc_;
  const Cronet_Engine_StopNetLogFunc StopNetLogFunc_;
  const Cronet_Engine_ShutdownFunc ShutdownFunc_;
  const Cronet_Engine_GetVersionStringFunc GetVersionStringFunc_;
  const Cronet_Engine_GetDefaultUserAgentFunc GetDefaultUserAgentFunc_;
  const Cronet_Engine_AddRequestFinishedListenerFunc
      AddRequestFinishedListenerFunc_;
  const Cronet_Engine_RemoveRequestFinishedListenerFunc
      RemoveRequestFinishedListenerFunc_;
};

Cronet_EnginePtr Cronet_Engine_CreateWith(
    Cronet_Engine_StartWithParamsFunc StartWithParamsFunc,
    Cronet_Engine_StartNetLogToFileFunc StartNetLogToFileFunc,
    Cronet_Engine_StopNetLogFunc StopNetLogFunc,
    Cronet_Engine_ShutdownFunc ShutdownFunc,
    Cronet_Engine_GetVersionStringFunc GetVersionStringFunc,
    Cronet_Engine_GetDefaultUserAgentFunc GetDefaultUserAgentFunc,
    Cronet_Engine_AddRequestFinishedListenerFunc AddRequestFinishedListenerFunc,
    Cronet_Engine_RemoveRequestFinishedListenerFunc
        RemoveRequestFinishedListenerFunc) {
  return new Cronet_EngineStub(
      StartWithParamsFunc, StartNetLogToFileFunc, StopNetLogFunc, ShutdownFunc,
      GetVersionStringFunc, GetDefaultUserAgentFunc,
      AddRequestFinishedListenerFunc, RemoveRequestFinishedListenerFunc);
}

// C functions of Cronet_UrlRequestStatusListener that forward calls to C++
// implementation.
void Cronet_UrlRequestStatusListener_Destroy(
    Cronet_UrlRequestStatusListenerPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UrlRequestStatusListener_SetClientContext(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_UrlRequestStatusListener_GetClientContext(
    Cronet_UrlRequestStatusListenerPtr self) {
  DCHECK(self);
  return self->client_context();
}

void Cronet_UrlRequestStatusListener_OnStatus(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_UrlRequestStatusListener_Status status) {
  DCHECK(self);
  self->OnStatus(status);
}

// Implementation of Cronet_UrlRequestStatusListener that forwards calls to C
// functions implemented by the app.
class Cronet_UrlRequestStatusListenerStub
    : public Cronet_UrlRequestStatusListener {
 public:
  explicit Cronet_UrlRequestStatusListenerStub(
      Cronet_UrlRequestStatusListener_OnStatusFunc OnStatusFunc)
      : OnStatusFunc_(OnStatusFunc) {}

  Cronet_UrlRequestStatusListenerStub(
      const Cronet_UrlRequestStatusListenerStub&) = delete;
  Cronet_UrlRequestStatusListenerStub& operator=(
      const Cronet_UrlRequestStatusListenerStub&) = delete;

  ~Cronet_UrlRequestStatusListenerStub() override {}

 protected:
  void OnStatus(Cronet_UrlRequestStatusListener_Status status) override {
    OnStatusFunc_(this, status);
  }

 private:
  const Cronet_UrlRequestStatusListener_OnStatusFunc OnStatusFunc_;
};

Cronet_UrlRequestStatusListenerPtr Cronet_UrlRequestStatusListener_CreateWith(
    Cronet_UrlRequestStatusListener_OnStatusFunc OnStatusFunc) {
  return new Cronet_UrlRequestStatusListenerStub(OnStatusFunc);
}

// C functions of Cronet_UrlRequestCallback that forward calls to C++
// implementation.
void Cronet_UrlRequestCallback_Destroy(Cronet_UrlRequestCallbackPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UrlRequestCallback_SetClientContext(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_UrlRequestCallback_GetClientContext(
    Cronet_UrlRequestCallbackPtr self) {
  DCHECK(self);
  return self->client_context();
}

void Cronet_UrlRequestCallback_OnRedirectReceived(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_String new_location_url) {
  DCHECK(self);
  self->OnRedirectReceived(request, info, new_location_url);
}

void Cronet_UrlRequestCallback_OnResponseStarted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  DCHECK(self);
  self->OnResponseStarted(request, info);
}

void Cronet_UrlRequestCallback_OnReadCompleted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_BufferPtr buffer,
    uint64_t bytes_read) {
  DCHECK(self);
  self->OnReadCompleted(request, info, buffer, bytes_read);
}

void Cronet_UrlRequestCallback_OnSucceeded(Cronet_UrlRequestCallbackPtr self,
                                           Cronet_UrlRequestPtr request,
                                           Cronet_UrlResponseInfoPtr info) {
  DCHECK(self);
  self->OnSucceeded(request, info);
}

void Cronet_UrlRequestCallback_OnFailed(Cronet_UrlRequestCallbackPtr self,
                                        Cronet_UrlRequestPtr request,
                                        Cronet_UrlResponseInfoPtr info,
                                        Cronet_ErrorPtr error) {
  DCHECK(self);
  self->OnFailed(request, info, error);
}

void Cronet_UrlRequestCallback_OnCanceled(Cronet_UrlRequestCallbackPtr self,
                                          Cronet_UrlRequestPtr request,
                                          Cronet_UrlResponseInfoPtr info) {
  DCHECK(self);
  self->OnCanceled(request, info);
}

// Implementation of Cronet_UrlRequestCallback that forwards calls to C
// functions implemented by the app.
class Cronet_UrlRequestCallbackStub : public Cronet_UrlRequestCallback {
 public:
  Cronet_UrlRequestCallbackStub(
      Cronet_UrlRequestCallback_OnRedirectReceivedFunc OnRedirectReceivedFunc,
      Cronet_UrlRequestCallback_OnResponseStartedFunc OnResponseStartedFunc,
      Cronet_UrlRequestCallback_OnReadCompletedFunc OnReadCompletedFunc,
      Cronet_UrlRequestCallback_OnSucceededFunc OnSucceededFunc,
      Cronet_UrlRequestCallback_OnFailedFunc OnFailedFunc,
      Cronet_UrlRequestCallback_OnCanceledFunc OnCanceledFunc)
      : OnRedirectReceivedFunc_(OnRedirectReceivedFunc),
        OnResponseStartedFunc_(OnResponseStartedFunc),
        OnReadCompletedFunc_(OnReadCompletedFunc),
        OnSucceededFunc_(OnSucceededFunc),
        OnFailedFunc_(OnFailedFunc),
        OnCanceledFunc_(OnCanceledFunc) {}

  Cronet_UrlRequestCallbackStub(const Cronet_UrlRequestCallbackStub&) = delete;
  Cronet_UrlRequestCallbackStub& operator=(
      const Cronet_UrlRequestCallbackStub&) = delete;

  ~Cronet_UrlRequestCallbackStub() override {}

 protected:
  void OnRedirectReceived(Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info,
                          Cronet_String new_location_url) override {
    OnRedirectReceivedFunc_(this, request, info, new_location_url);
  }

  void OnResponseStarted(Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info) override {
    OnResponseStartedFunc_(this, request, info);
  }

  void OnReadCompleted(Cronet_UrlRequestPtr request,
                       Cronet_UrlResponseInfoPtr info,
                       Cronet_BufferPtr buffer,
                       uint64_t bytes_read) override {
    OnReadCompletedFunc_(this, request, info, buffer, bytes_read);
  }

  void OnSucceeded(Cronet_UrlRequestPtr request,
                   Cronet_UrlResponseInfoPtr info) override {
    OnSucceededFunc_(this, request, info);
  }

  void OnFailed(Cronet_UrlRequestPtr request,
                Cronet_UrlResponseInfoPtr info,
                Cronet_ErrorPtr error) override {
    OnFailedFunc_(this, request, info, error);
  }

  void OnCanceled(Cronet_UrlRequestPtr request,
                  Cronet_UrlResponseInfoPtr info) override {
    OnCanceledFunc_(this, request, info);
  }

 private:
  const Cronet_UrlRequestCallback_OnRedirectReceivedFunc
      OnRedirectReceivedFunc_;
  const Cronet_UrlRequestCallback_OnResponseStartedFunc OnResponseStartedFunc_;
  const Cronet_UrlRequestCallback_OnReadCompletedFunc OnReadCompletedFunc_;
  const Cronet_UrlRequestCallback_OnSucceededFunc OnSucceededFunc_;
  const Cronet_UrlRequestCallback_OnFailedFunc OnFailedFunc_;
  const Cronet_UrlRequestCallback_OnCanceledFunc OnCanceledFunc_;
};

Cronet_UrlRequestCallbackPtr Cronet_UrlRequestCallback_CreateWith(
    Cronet_UrlRequestCallback_OnRedirectReceivedFunc OnRedirectReceivedFunc,
    Cronet_UrlRequestCallback_OnResponseStartedFunc OnResponseStartedFunc,
    Cronet_UrlRequestCallback_OnReadCompletedFunc OnReadCompletedFunc,
    Cronet_UrlRequestCallback_OnSucceededFunc OnSucceededFunc,
    Cronet_UrlRequestCallback_OnFailedFunc OnFailedFunc,
    Cronet_UrlRequestCallback_OnCanceledFunc OnCanceledFunc) {
  return new Cronet_UrlRequestCallbackStub(
      OnRedirectReceivedFunc, OnResponseStartedFunc, OnReadCompletedFunc,
      OnSucceededFunc, OnFailedFunc, OnCanceledFunc);
}

// C functions of Cronet_UploadDataSink that forward calls to C++
// implementation.
void Cronet_UploadDataSink_Destroy(Cronet_UploadDataSinkPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UploadDataSink_SetClientContext(
    Cronet_UploadDataSinkPtr self,
    Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_UploadDataSink_GetClientContext(
    Cronet_UploadDataSinkPtr self) {
  DCHECK(self);
  return self->client_context();
}

void Cronet_UploadDataSink_OnReadSucceeded(Cronet_UploadDataSinkPtr self,
                                           uint64_t bytes_read,
                                           bool final_chunk) {
  DCHECK(self);
  self->OnReadSucceeded(bytes_read, final_chunk);
}

void Cronet_UploadDataSink_OnReadError(Cronet_UploadDataSinkPtr self,
                                       Cronet_String error_message) {
  DCHECK(self);
  self->OnReadError(error_message);
}

void Cronet_UploadDataSink_OnRewindSucceeded(Cronet_UploadDataSinkPtr self) {
  DCHECK(self);
  self->OnRewindSucceeded();
}

void Cronet_UploadDataSink_OnRewindError(Cronet_UploadDataSinkPtr self,
                                         Cronet_String error_message) {
  DCHECK(self);
  self->OnRewindError(error_message);
}

// Implementation of Cronet_UploadDataSink that forwards calls to C functions
// implemented by the app.
class Cronet_UploadDataSinkStub : public Cronet_UploadDataSink {
 public:
  Cronet_UploadDataSinkStub(
      Cronet_UploadDataSink_OnReadSucceededFunc OnReadSucceededFunc,
      Cronet_UploadDataSink_OnReadErrorFunc OnReadErrorFunc,
      Cronet_UploadDataSink_OnRewindSucceededFunc OnRewindSucceededFunc,
      Cronet_UploadDataSink_OnRewindErrorFunc OnRewindErrorFunc)
      : OnReadSucceededFunc_(OnReadSucceededFunc),
        OnReadErrorFunc_(OnReadErrorFunc),
        OnRewindSucceededFunc_(OnRewindSucceededFunc),
        OnRewindErrorFunc_(OnRewindErrorFunc) {}

  Cronet_UploadDataSinkStub(const Cronet_UploadDataSinkStub&) = delete;
  Cronet_UploadDataSinkStub& operator=(const Cronet_UploadDataSinkStub&) =
      delete;

  ~Cronet_UploadDataSinkStub() override {}

 protected:
  void OnReadSucceeded(uint64_t bytes_read, bool final_chunk) override {
    OnReadSucceededFunc_(this, bytes_read, final_chunk);
  }

  void OnReadError(Cronet_String error_message) override {
    OnReadErrorFunc_(this, error_message);
  }

  void OnRewindSucceeded() override { OnRewindSucceededFunc_(this); }

  void OnRewindError(Cronet_String error_message) override {
    OnRewindErrorFunc_(this, error_message);
  }

 private:
  const Cronet_UploadDataSink_OnReadSucceededFunc OnReadSucceededFunc_;
  const Cronet_UploadDataSink_OnReadErrorFunc OnReadErrorFunc_;
  const Cronet_UploadDataSink_OnRewindSucceededFunc OnRewindSucceededFunc_;
  const Cronet_UploadDataSink_OnRewindErrorFunc OnRewindErrorFunc_;
};

Cronet_UploadDataSinkPtr Cronet_UploadDataSink_CreateWith(
    Cronet_UploadDataSink_OnReadSucceededFunc OnReadSucceededFunc,
    Cronet_UploadDataSink_OnReadErrorFunc OnReadErrorFunc,
    Cronet_UploadDataSink_OnRewindSucceededFunc OnRewindSucceededFunc,
    Cronet_UploadDataSink_OnRewindErrorFunc OnRewindErrorFunc) {
  return new Cronet_UploadDataSinkStub(OnReadSucceededFunc, OnReadErrorFunc,
                                       OnRewindSucceededFunc,
                                       OnRewindErrorFunc);
}

// C functions of Cronet_UploadDataProvider that forward calls to C++
// implementation.
void Cronet_UploadDataProvider_Destroy(Cronet_UploadDataProviderPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UploadDataProvider_SetClientContext(
    Cronet_UploadDataProviderPtr self,
    Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_UploadDataProvider_GetClientContext(
    Cronet_UploadDataProviderPtr self) {
  DCHECK(self);
  return self->client_context();
}

int64_t Cronet_UploadDataProvider_GetLength(Cronet_UploadDataProviderPtr self) {
  DCHECK(self);
  return self->GetLength();
}

void Cronet_UploadDataProvider_Read(Cronet_UploadDataProviderPtr self,
                                    Cronet_UploadDataSinkPtr upload_data_sink,
                                    Cronet_BufferPtr buffer) {
  DCHECK(self);
  self->Read(upload_data_sink, buffer);
}

void Cronet_UploadDataProvider_Rewind(
    Cronet_UploadDataProviderPtr self,
    Cronet_UploadDataSinkPtr upload_data_sink) {
  DCHECK(self);
  self->Rewind(upload_data_sink);
}

void Cronet_UploadDataProvider_Close(Cronet_UploadDataProviderPtr self) {
  DCHECK(self);
  self->Close();
}

// Implementation of Cronet_UploadDataProvider that forwards calls to C
// functions implemented by the app.
class Cronet_UploadDataProviderStub : public Cronet_UploadDataProvider {
 public:
  Cronet_UploadDataProviderStub(
      Cronet_UploadDataProvider_GetLengthFunc GetLengthFunc,
      Cronet_UploadDataProvider_ReadFunc ReadFunc,
      Cronet_UploadDataProvider_RewindFunc RewindFunc,
      Cronet_UploadDataProvider_CloseFunc CloseFunc)
      : GetLengthFunc_(GetLengthFunc),
        ReadFunc_(ReadFunc),
        RewindFunc_(RewindFunc),
        CloseFunc_(CloseFunc) {}

  Cronet_UploadDataProviderStub(const Cronet_UploadDataProviderStub&) = delete;
  Cronet_UploadDataProviderStub& operator=(
      const Cronet_UploadDataProviderStub&) = delete;

  ~Cronet_UploadDataProviderStub() override {}

 protected:
  int64_t GetLength() override { return GetLengthFunc_(this); }

  void Read(Cronet_UploadDataSinkPtr upload_data_sink,
            Cronet_BufferPtr buffer) override {
    ReadFunc_(this, upload_data_sink, buffer);
  }

  void Rewind(Cronet_UploadDataSinkPtr upload_data_sink) override {
    RewindFunc_(this, upload_data_sink);
  }

  void Close() override { CloseFunc_(this); }

 private:
  const Cronet_UploadDataProvider_GetLengthFunc GetLengthFunc_;
  const Cronet_UploadDataProvider_ReadFunc ReadFunc_;
  const Cronet_UploadDataProvider_RewindFunc RewindFunc_;
  const Cronet_UploadDataProvider_CloseFunc CloseFunc_;
};

Cronet_UploadDataProviderPtr Cronet_UploadDataProvider_CreateWith(
    Cronet_UploadDataProvider_GetLengthFunc GetLengthFunc,
    Cronet_UploadDataProvider_ReadFunc ReadFunc,
    Cronet_UploadDataProvider_RewindFunc RewindFunc,
    Cronet_UploadDataProvider_CloseFunc CloseFunc) {
  return new Cronet_UploadDataProviderStub(GetLengthFunc, ReadFunc, RewindFunc,
                                           CloseFunc);
}

// C functions of Cronet_UrlRequest that forward calls to C++ implementation.
void Cronet_UrlRequest_Destroy(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UrlRequest_SetClientContext(Cronet_UrlRequestPtr self,
                                        Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_UrlRequest_GetClientContext(
    Cronet_UrlRequestPtr self) {
  DCHECK(self);
  return self->client_context();
}

Cronet_RESULT Cronet_UrlRequest_InitWithParams(
    Cronet_UrlRequestPtr self,
    Cronet_EnginePtr engine,
    Cronet_String url,
    Cronet_UrlRequestParamsPtr params,
    Cronet_UrlRequestCallbackPtr callback,
    Cronet_ExecutorPtr executor) {
  DCHECK(self);
  return self->InitWithParams(engine, url, params, callback, executor);
}

Cronet_RESULT Cronet_UrlRequest_Start(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  return self->Start();
}

Cronet_RESULT Cronet_UrlRequest_FollowRedirect(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  return self->FollowRedirect();
}

Cronet_RESULT Cronet_UrlRequest_Read(Cronet_UrlRequestPtr self,
                                     Cronet_BufferPtr buffer) {
  DCHECK(self);
  return self->Read(buffer);
}

void Cronet_UrlRequest_Cancel(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  self->Cancel();
}

bool Cronet_UrlRequest_IsDone(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  return self->IsDone();
}

void Cronet_UrlRequest_GetStatus(Cronet_UrlRequestPtr self,
                                 Cronet_UrlRequestStatusListenerPtr listener) {
  DCHECK(self);
  self->GetStatus(listener);
}

// Implementation of Cronet_UrlRequest that forwards calls to C functions
// implemented by the app.
class Cronet_UrlRequestStub : public Cronet_UrlRequest {
 public:
  Cronet_UrlRequestStub(Cronet_UrlRequest_InitWithParamsFunc InitWithParamsFunc,
                        Cronet_UrlRequest_StartFunc StartFunc,
                        Cronet_UrlRequest_FollowRedirectFunc FollowRedirectFunc,
                        Cronet_UrlRequest_ReadFunc ReadFunc,
                        Cronet_UrlRequest_CancelFunc CancelFunc,
                        Cronet_UrlRequest_IsDoneFunc IsDoneFunc,
                        Cronet_UrlRequest_GetStatusFunc GetStatusFunc)
      : InitWithParamsFunc_(InitWithParamsFunc),
        StartFunc_(StartFunc),
        FollowRedirectFunc_(FollowRedirectFunc),
        ReadFunc_(ReadFunc),
        CancelFunc_(CancelFunc),
        IsDoneFunc_(IsDoneFunc),
        GetStatusFunc_(GetStatusFunc) {}

  Cronet_UrlRequestStub(const Cronet_UrlRequestStub&) = delete;
  Cronet_UrlRequestStub& operator=(const Cronet_UrlRequestStub&) = delete;

  ~Cronet_UrlRequestStub() override {}

 protected:
  Cronet_RESULT InitWithParams(Cronet_EnginePtr engine,
                               Cronet_String url,
                               Cronet_UrlRequestParamsPtr params,
                               Cronet_UrlRequestCallbackPtr callback,
                               Cronet_ExecutorPtr executor) override {
    return InitWithParamsFunc_(this, engine, url, params, callback, executor);
  }

  Cronet_RESULT Start() override { return StartFunc_(this); }

  Cronet_RESULT FollowRedirect() override { return FollowRedirectFunc_(this); }

  Cronet_RESULT Read(Cronet_BufferPtr buffer) override {
    return ReadFunc_(this, buffer);
  }

  void Cancel() override { CancelFunc_(this); }

  bool IsDone() override { return IsDoneFunc_(this); }

  void GetStatus(Cronet_UrlRequestStatusListenerPtr listener) override {
    GetStatusFunc_(this, listener);
  }

 private:
  const Cronet_UrlRequest_InitWithParamsFunc InitWithParamsFunc_;
  const Cronet_UrlRequest_StartFunc StartFunc_;
  const Cronet_UrlRequest_FollowRedirectFunc FollowRedirectFunc_;
  const Cronet_UrlRequest_ReadFunc ReadFunc_;
  const Cronet_UrlRequest_CancelFunc CancelFunc_;
  const Cronet_UrlRequest_IsDoneFunc IsDoneFunc_;
  const Cronet_UrlRequest_GetStatusFunc GetStatusFunc_;
};

Cronet_UrlRequestPtr Cronet_UrlRequest_CreateWith(
    Cronet_UrlRequest_InitWithParamsFunc InitWithParamsFunc,
    Cronet_UrlRequest_StartFunc StartFunc,
    Cronet_UrlRequest_FollowRedirectFunc FollowRedirectFunc,
    Cronet_UrlRequest_ReadFunc ReadFunc,
    Cronet_UrlRequest_CancelFunc CancelFunc,
    Cronet_UrlRequest_IsDoneFunc IsDoneFunc,
    Cronet_UrlRequest_GetStatusFunc GetStatusFunc) {
  return new Cronet_UrlRequestStub(InitWithParamsFunc, StartFunc,
                                   FollowRedirectFunc, ReadFunc, CancelFunc,
                                   IsDoneFunc, GetStatusFunc);
}

// C functions of Cronet_RequestFinishedInfoListener that forward calls to C++
// implementation.
void Cronet_RequestFinishedInfoListener_Destroy(
    Cronet_RequestFinishedInfoListenerPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_RequestFinishedInfoListener_SetClientContext(
    Cronet_RequestFinishedInfoListenerPtr self,
    Cronet_ClientContext client_context) {
  DCHECK(self);
  self->set_client_context(client_context);
}

Cronet_ClientContext Cronet_RequestFinishedInfoListener_GetClientContext(
    Cronet_RequestFinishedInfoListenerPtr self) {
  DCHECK(self);
  return self->client_context();
}

void Cronet_RequestFinishedInfoListener_OnRequestFinished(
    Cronet_RequestFinishedInfoListenerPtr self,
    Cronet_RequestFinishedInfoPtr request_info,
    Cronet_UrlResponseInfoPtr response_info,
    Cronet_ErrorPtr error) {
  DCHECK(self);
  self->OnRequestFinished(request_info, response_info, error);
}

// Implementation of Cronet_RequestFinishedInfoListener that forwards calls to C
// functions implemented by the app.
class Cronet_RequestFinishedInfoListenerStub
    : public Cronet_RequestFinishedInfoListener {
 public:
  explicit Cronet_RequestFinishedInfoListenerStub(
      Cronet_RequestFinishedInfoListener_OnRequestFinishedFunc
          OnRequestFinishedFunc)
      : OnRequestFinishedFunc_(OnRequestFinishedFunc) {}

  Cronet_RequestFinishedInfoListenerStub(
      const Cronet_RequestFinishedInfoListenerStub&) = delete;
  Cronet_RequestFinishedInfoListenerStub& operator=(
      const Cronet_RequestFinishedInfoListenerStub&) = delete;

  ~Cronet_RequestFinishedInfoListenerStub() override {}

 protected:
  void OnRequestFinished(Cronet_RequestFinishedInfoPtr request_info,
                         Cronet_UrlResponseInfoPtr response_info,
                         Cronet_ErrorPtr error) override {
    OnRequestFinishedFunc_(this, request_info, response_info, error);
  }

 private:
  const Cronet_RequestFinishedInfoListener_OnRequestFinishedFunc
      OnRequestFinishedFunc_;
};

Cronet_RequestFinishedInfoListenerPtr
Cronet_RequestFinishedInfoListener_CreateWith(
    Cronet_RequestFinishedInfoListener_OnRequestFinishedFunc
        OnRequestFinishedFunc) {
  return new Cronet_RequestFinishedInfoListenerStub(OnRequestFinishedFunc);
}
