// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_
#define CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/renderer/media/cast_session_delegate.h"
#include "net/base/ip_endpoint.h"

namespace base {
class DictionaryValue;
}  // namespace base

class CastSession;

// This class represents the transport mechanism used by Cast RTP streams
// to connect to a remote client. It specifies the destination address
// and network protocol used to send Cast RTP streams.
class CastUdpTransport {
 public:
  explicit CastUdpTransport(const scoped_refptr<CastSession>& session);
  virtual ~CastUdpTransport();

  // Specify the remote IP address and port.
  void SetDestination(const net::IPEndPoint& remote_address,
                      const CastSessionDelegate::ErrorCallback& error_callback);

  // Set options.
  void SetOptions(std::unique_ptr<base::DictionaryValue> options);

 private:
  const scoped_refptr<CastSession> cast_session_;
  net::IPEndPoint remote_address_;
  std::unique_ptr<base::DictionaryValue> options_;
  base::WeakPtrFactory<CastUdpTransport> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastUdpTransport);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_UDP_TRANSPORT_H_
