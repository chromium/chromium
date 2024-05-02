// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPEECH_DOWNSTREAM_LOADER_CLIENT_H_
#define COMPONENTS_SPEECH_DOWNSTREAM_LOADER_CLIENT_H_

namespace speech {

// An interface containing the callback functions required by consumers
// of the DownstreamLoader. The class that implements this client
// interface must outlive the DownstreamLoader.
class DownstreamLoaderClient {
 public:
  DownstreamLoaderClient(const DownstreamLoaderClient&) = delete;
  DownstreamLoaderClient& operator=(const DownstreamLoaderClient&) = delete;

 protected:
  DownstreamLoaderClient() = default;
  virtual ~DownstreamLoaderClient() = default;

 private:
  friend class DownstreamLoader;

  // Executed when downstream data is received.
  virtual void OnDownstreamDataReceived(std::string_view new_response_data) = 0;

  // Executed when downstream data is completed.
  // success: True on 2xx responses where the entire body was successfully
  // received. response_code: The HTTP response code if available, or -1 on
  // network errors.
  virtual void OnDownstreamDataComplete(bool success, int response_code) = 0;
};

}  // namespace speech

#endif  // COMPONENTS_SPEECH_DOWNSTREAM_LOADER_CLIENT_H_
