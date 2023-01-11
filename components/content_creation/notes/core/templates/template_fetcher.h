// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_FETCHER_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_FETCHER_H_

#include "base/functional/callback.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

namespace content_creation {

using FetchTemplateCompleteCallback = base::OnceCallback<void(std::string)>;

constexpr char kTemplateUrl[] =
    "https://www.gstatic.com/chrome/content/webnotes/templates/"
    "templates.data";

// This class fetches the template data used for WebNotes Stylized
// which can then be converted to NoteTemplate objects and
// served to users.
class TemplateFetcher {
 public:
  explicit TemplateFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);
  ~TemplateFetcher();

  TemplateFetcher(const TemplateFetcher&) = delete;
  TemplateFetcher& operator=(const TemplateFetcher&) = delete;

  // Begins the GET request and calls OnTemplateFetchComplete whether the GET
  // request goes through or fails.
  void Start(FetchTemplateCompleteCallback callback);

 private:
  std::unique_ptr<network::ResourceRequest> CreateTemplateResourceRequest();

  void OnTemplateFetchComplete(FetchTemplateCompleteCallback callback,
                               std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<TemplateFetcher> weak_factory_{this};
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_FETCHER_H_
