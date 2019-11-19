// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CAST_STREAMING_NATIVE_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_CAST_STREAMING_NATIVE_HANDLER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8.h"

class CastRtpStream;
class CastUdpTransport;

namespace base {
class DictionaryValue;
class Value;
}

namespace net {
class IPEndPoint;
}

namespace extensions {
class NativeExtensionBindingsSystem;

// Native code that handle chrome.webrtc custom bindings.
class CastStreamingNativeHandler : public ObjectBackedNativeHandler {
 public:
  CastStreamingNativeHandler(ScriptContext* context,
                             NativeExtensionBindingsSystem* bindings_system);
  ~CastStreamingNativeHandler() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 protected:
  // Shut down all sessions and cancel any in-progress operations because the
  // ScriptContext is about to become invalid.
  void Invalidate() override;

 private:
  void CreateCastSession(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void DestroyCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void CreateParamsCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetSupportedParamsCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args) const;
  void StartCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StopCastRtpStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void DestroyCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void SetDestinationCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void SetOptionsCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  void StopCastUdpTransport(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void ToggleLogging(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetRawEvents(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetStats(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Helper method to call the v8 callback function after a session is
  // created.
  void CallCreateCallback(std::unique_ptr<CastRtpStream> stream1,
                          std::unique_ptr<CastRtpStream> stream2,
                          std::unique_ptr<CastUdpTransport> udp_transport);

  void CallStartCallback(int stream_id) const;
  void CallStopCallback(int stream_id) const;
  void CallErrorCallback(int stream_id, const std::string& message) const;

  // |function| is a javascript function that will take |error_message| as
  // an argument. Called when something goes wrong in a cast receiver.
  void CallReceiverErrorCallback(
      v8::CopyablePersistentTraits<v8::Function>::CopyablePersistent function,
      const std::string& error_message);

  void CallGetRawEventsCallback(int transport_id,
                                std::unique_ptr<base::Value> raw_events);
  void CallGetStatsCallback(int transport_id,
                            std::unique_ptr<base::DictionaryValue> stats);

  // Gets the RTP stream or UDP transport indexed by an ID.
  // If not found, returns NULL and throws a V8 exception.
  CastRtpStream* GetRtpStreamOrThrow(int stream_id) const;
  CastUdpTransport* GetUdpTransportOrThrow(int transport_id) const;

  bool IPEndPointFromArg(v8::Isolate* isolate,
                         const v8::Local<v8::Value>& arg,
                         net::IPEndPoint* ip_endpoint) const;

  int last_transport_id_;

  using RtpStreamMap = std::map<int, std::unique_ptr<CastRtpStream>>;
  RtpStreamMap rtp_stream_map_;

  using UdpTransportMap = std::map<int, std::unique_ptr<CastUdpTransport>>;
  UdpTransportMap udp_transport_map_;

  v8::Global<v8::Function> create_callback_;

  using RtpStreamCallbackMap = std::map<int, v8::Global<v8::Function>>;
  RtpStreamCallbackMap get_raw_events_callbacks_;
  RtpStreamCallbackMap get_stats_callbacks_;

  NativeExtensionBindingsSystem* bindings_system_;

  base::WeakPtrFactory<CastStreamingNativeHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastStreamingNativeHandler);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CAST_STREAMING_NATIVE_HANDLER_H_
