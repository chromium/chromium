// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#ifndef COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_INTERFACE_H_
#define COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_INTERFACE_H_

#include "components/cronet/native/generated/cronet.idl_c.h"

struct Cronet_Buffer {
  Cronet_Buffer() = default;

  Cronet_Buffer(const Cronet_Buffer&) = delete;
  Cronet_Buffer& operator=(const Cronet_Buffer&) = delete;

  virtual ~Cronet_Buffer() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual void InitWithDataAndCallback(Cronet_RawDataPtr data,
                                       uint64_t size,
                                       Cronet_BufferCallbackPtr callback) = 0;
  virtual void InitWithAlloc(uint64_t size) = 0;
  virtual uint64_t GetSize() = 0;
  virtual Cronet_RawDataPtr GetData() = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_BufferCallback {
  Cronet_BufferCallback() = default;

  Cronet_BufferCallback(const Cronet_BufferCallback&) = delete;
  Cronet_BufferCallback& operator=(const Cronet_BufferCallback&) = delete;

  virtual ~Cronet_BufferCallback() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual void OnDestroy(Cronet_BufferPtr buffer) = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_Runnable {
  Cronet_Runnable() = default;

  Cronet_Runnable(const Cronet_Runnable&) = delete;
  Cronet_Runnable& operator=(const Cronet_Runnable&) = delete;

  virtual ~Cronet_Runnable() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual void Run() = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_Executor {
  Cronet_Executor() = default;

  Cronet_Executor(const Cronet_Executor&) = delete;
  Cronet_Executor& operator=(const Cronet_Executor&) = delete;

  virtual ~Cronet_Executor() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual void Execute(Cronet_RunnablePtr command) = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_Engine {
  Cronet_Engine() = default;

  Cronet_Engine(const Cronet_Engine&) = delete;
  Cronet_Engine& operator=(const Cronet_Engine&) = delete;

  virtual ~Cronet_Engine() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual Cronet_RESULT StartWithParams(Cronet_EngineParamsPtr params) = 0;
  virtual bool StartNetLogToFile(Cronet_String file_name, bool log_all) = 0;
  virtual void StopNetLog() = 0;
  virtual Cronet_RESULT Shutdown() = 0;
  virtual Cronet_String GetVersionString() = 0;
  virtual Cronet_String GetDefaultUserAgent() = 0;
  virtual void AddRequestFinishedListener(
      Cronet_RequestFinishedInfoListenerPtr listener,
      Cronet_ExecutorPtr executor) = 0;
  virtual void RemoveRequestFinishedListener(
      Cronet_RequestFinishedInfoListenerPtr listener) = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_UrlRequestStatusListener {
  Cronet_UrlRequestStatusListener() = default;

  Cronet_UrlRequestStatusListener(const Cronet_UrlRequestStatusListener&) =
      delete;
  Cronet_UrlRequestStatusListener& operator=(
      const Cronet_UrlRequestStatusListener&) = delete;

  virtual ~Cronet_UrlRequestStatusListener() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual void OnStatus(Cronet_UrlRequestStatusListener_Status status) = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_UrlRequestCallback {
  Cronet_UrlRequestCallback() = default;

  Cronet_UrlRequestCallback(const Cronet_UrlRequestCallback&) = delete;
  Cronet_UrlRequestCallback& operator=(const Cronet_UrlRequestCallback&) =
      delete;

  virtual ~Cronet_UrlRequestCallback() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual void OnRedirectReceived(Cronet_UrlRequestPtr request,
                                  Cronet_UrlResponseInfoPtr info,
                                  Cronet_String new_location_url) = 0;
  virtual void OnResponseStarted(Cronet_UrlRequestPtr request,
                                 Cronet_UrlResponseInfoPtr info) = 0;
  virtual void OnReadCompleted(Cronet_UrlRequestPtr request,
                               Cronet_UrlResponseInfoPtr info,
                               Cronet_BufferPtr buffer,
                               uint64_t bytes_read) = 0;
  virtual void OnSucceeded(Cronet_UrlRequestPtr request,
                           Cronet_UrlResponseInfoPtr info) = 0;
  virtual void OnFailed(Cronet_UrlRequestPtr request,
                        Cronet_UrlResponseInfoPtr info,
                        Cronet_ErrorPtr error) = 0;
  virtual void OnCanceled(Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info) = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_UploadDataSink {
  Cronet_UploadDataSink() = default;

  Cronet_UploadDataSink(const Cronet_UploadDataSink&) = delete;
  Cronet_UploadDataSink& operator=(const Cronet_UploadDataSink&) = delete;

  virtual ~Cronet_UploadDataSink() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual void OnReadSucceeded(uint64_t bytes_read, bool final_chunk) = 0;
  virtual void OnReadError(Cronet_String error_message) = 0;
  virtual void OnRewindSucceeded() = 0;
  virtual void OnRewindError(Cronet_String error_message) = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_UploadDataProvider {
  Cronet_UploadDataProvider() = default;

  Cronet_UploadDataProvider(const Cronet_UploadDataProvider&) = delete;
  Cronet_UploadDataProvider& operator=(const Cronet_UploadDataProvider&) =
      delete;

  virtual ~Cronet_UploadDataProvider() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual int64_t GetLength() = 0;
  virtual void Read(Cronet_UploadDataSinkPtr upload_data_sink,
                    Cronet_BufferPtr buffer) = 0;
  virtual void Rewind(Cronet_UploadDataSinkPtr upload_data_sink) = 0;
  virtual void Close() = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_UrlRequest {
  Cronet_UrlRequest() = default;

  Cronet_UrlRequest(const Cronet_UrlRequest&) = delete;
  Cronet_UrlRequest& operator=(const Cronet_UrlRequest&) = delete;

  virtual ~Cronet_UrlRequest() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual Cronet_RESULT InitWithParams(Cronet_EnginePtr engine,
                                       Cronet_String url,
                                       Cronet_UrlRequestParamsPtr params,
                                       Cronet_UrlRequestCallbackPtr callback,
                                       Cronet_ExecutorPtr executor) = 0;
  virtual Cronet_RESULT Start() = 0;
  virtual Cronet_RESULT FollowRedirect() = 0;
  virtual Cronet_RESULT Read(Cronet_BufferPtr buffer) = 0;
  virtual void Cancel() = 0;
  virtual bool IsDone() = 0;
  virtual void GetStatus(Cronet_UrlRequestStatusListenerPtr listener) = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

struct Cronet_RequestFinishedInfoListener {
  Cronet_RequestFinishedInfoListener() = default;

  Cronet_RequestFinishedInfoListener(
      const Cronet_RequestFinishedInfoListener&) = delete;
  Cronet_RequestFinishedInfoListener& operator=(
      const Cronet_RequestFinishedInfoListener&) = delete;

  virtual ~Cronet_RequestFinishedInfoListener() = default;

  void set_client_context(Cronet_ClientContext client_context) {
    client_context_ = client_context;
  }
  Cronet_ClientContext client_context() const { return client_context_; }

  virtual void OnRequestFinished(Cronet_RequestFinishedInfoPtr request_info,
                                 Cronet_UrlResponseInfoPtr response_info,
                                 Cronet_ErrorPtr error) = 0;

 private:
  Cronet_ClientContext client_context_ = nullptr;
};

#endif  // COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_INTERFACE_H_
