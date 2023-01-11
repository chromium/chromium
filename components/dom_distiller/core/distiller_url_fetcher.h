// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_URL_FETCHER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_URL_FETCHER_H_

#include <string>

#include "base/functional/callback.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace dom_distiller {

class DistillerURLFetcher;

// Class for creating a DistillerURLFetcher.
class DistillerURLFetcherFactory {
 public:
  explicit DistillerURLFetcherFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~DistillerURLFetcherFactory();
  virtual DistillerURLFetcher* CreateDistillerURLFetcher() const;

 private:
  friend class TestDistillerURLFetcherFactory;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

// This class loads a URL, and notifies the caller when the operation
// completes or fails. If the request fails, an empty string will be returned.
class DistillerURLFetcher {
 public:
  explicit DistillerURLFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~DistillerURLFetcher();

  // Indicates when a fetch is done.
  using URLFetcherCallback = base::OnceCallback<void(const std::string& data)>;

  // Fetches a |url|. Notifies when the fetch is done via |callback|.
  virtual void FetchURL(const std::string& url, URLFetcherCallback callback);

  DistillerURLFetcher(const DistillerURLFetcher&) = delete;
  DistillerURLFetcher& operator=(const DistillerURLFetcher&) = delete;

 protected:
  virtual std::unique_ptr<network::SimpleURLLoader> CreateURLFetcher(
      const std::string& url);

 private:
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  URLFetcherCallback callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  //  namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_URL_FETCHER_H_
