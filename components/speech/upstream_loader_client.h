// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPEECH_UPSTREAM_LOADER_CLIENT_H_
#define COMPONENTS_SPEECH_UPSTREAM_LOADER_CLIENT_H_

namespace speech {

// An interface containing the callback functions required by consumers
// of the UpstreamLoader. The class that implements this client
// interface must outlive the UpstreamLoader.
class UpstreamLoaderClient {
 public:
  UpstreamLoaderClient(const UpstreamLoaderClient&) = delete;
  UpstreamLoaderClient& operator=(const UpstreamLoaderClient&) = delete;

 protected:
  UpstreamLoaderClient() = default;
  virtual ~UpstreamLoaderClient() = default;

 private:
  friend class UpstreamLoader;

  // Executed when upstream data is completed.
  // success: True on 2xx responses.
  // response_code: The HTTP response code if available, or -1 on
  // network errors.
  virtual void OnUpstreamDataComplete(bool success, int response_code) = 0;
};

}  // namespace speech

#endif  // COMPONENTS_SPEECH_UPSTREAM_LOADER_CLIENT_H_
