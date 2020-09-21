// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_URL_LOADER_IMPL_H_
#define CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_URL_LOADER_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/bloom/server/bloom_url_loader.h"

namespace network {
class PendingSharedURLLoaderFactory;
class SharedURLLoaderFactory;
}  // namespace network

namespace chromeos {
namespace bloom {

class BloomURLLoaderImpl : public BloomURLLoader {
 public:
  explicit BloomURLLoaderImpl(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          url_loader_factory);
  BloomURLLoaderImpl(BloomURLLoaderImpl&) = delete;
  BloomURLLoaderImpl& operator=(BloomURLLoaderImpl&) = delete;
  ~BloomURLLoaderImpl() override;

  void SendPostRequest(const GURL& url,
                       const std::string& access_token,
                       std::string&& body,
                       const std::string& mime_type,
                       Callback callback) override;
  void SendGetRequest(const GURL& url,
                      const std::string& access_token,
                      Callback callback) override;

 private:
  void OnServerResponse(Callback callback,
                        std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<BloomURLLoaderImpl> weak_ptr_factory_;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_URL_LOADER_IMPL_H_
