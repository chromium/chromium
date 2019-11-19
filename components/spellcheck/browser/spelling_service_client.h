// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLING_SERVICE_CLIENT_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLING_SERVICE_CLIENT_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

struct SpellCheckResult;

namespace content {
class BrowserContext;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// A class that encapsulates a JSON-RPC call to the Spelling service to check
// text there. This class creates a JSON-RPC request, sends the request to the
// service with URLFetcher, parses a response from the service, and calls a
// provided callback method. When a user deletes this object before it finishes
// a JSON-RPC call, this class cancels the JSON-RPC call without calling the
// callback method. A simple usage is creating a SpellingServiceClient and
// calling its RequestTextCheck method as listed in the following snippet.
//
//   class MyClient {
//    public:
//     MyClient();
//     virtual ~MyClient();
//
//     void OnTextCheckComplete(
//         int tag,
//         bool success,
//         const std::vector<SpellCheckResult>& results) {
//       ...
//     }
//
//     void MyTextCheck(BrowserContext* context, const base::string16& text) {
//        client_.reset(new SpellingServiceClient);
//        client_->RequestTextCheck(context, 0, text,
//            base::BindOnce(&MyClient::OnTextCheckComplete,
//                           base::Unretained(this));
//     }
//    private:
//     std::unique_ptr<SpellingServiceClient> client_;
//   };
//
class SpellingServiceClient {
 public:
  // Service types provided by the Spelling service. The Spelling service
  // consists of a couple of backends:
  // * SUGGEST: Retrieving suggestions for a word (used by Google Search), and;
  // * SPELLCHECK: Spellchecking text (used by Google Docs).
  // This type is used for choosing a backend when sending a JSON-RPC request to
  // the service.
  enum ServiceType {
    SUGGEST = 1,
    SPELLCHECK = 2,
  };
  // An enum to classify request responses. This is only used for metrics.
  // * REQUEST_FAILURE: The server returned an error.
  // * SUCCESS_EMPTY: The server returned an empty list of suggestions.
  // * SUCCESS_WITH_SUGGESTIONS: The server returned some suggestions.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ServiceRequestResultType : int {
    kRequestFailure = 0,
    kSuccessEmpty = 1,
    kSuccessWithSuggestions = 2,
    kMaxValue = kSuccessWithSuggestions,
  };
  typedef base::OnceCallback<void(
      bool /* success */,
      const base::string16& /* text */,
      const std::vector<SpellCheckResult>& /* results */)>
      TextCheckCompleteCallback;

  SpellingServiceClient();
  ~SpellingServiceClient();

  // Sends a text-check request to the Spelling service. When we send a request
  // to the Spelling service successfully, this function returns true. (This
  // does not mean the service finishes checking text successfully.) We will
  // call |callback| when we receive a text-check response from the service.
  bool RequestTextCheck(content::BrowserContext* context,
                        ServiceType type,
                        const base::string16& text,
                        TextCheckCompleteCallback callback);

  // Returns whether the specified service is available for the given context.
  static bool IsAvailable(content::BrowserContext* context, ServiceType type);

  // Set the URL loader factory for tests.
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_for_testing);

  // Builds the endpoint URL to use for the service request.
  GURL BuildEndpointUrl(int type);

 protected:
  // Parses a JSON-RPC response from the Spelling service.
  bool ParseResponse(const std::string& data,
                     std::vector<SpellCheckResult>* results);

 private:
  struct TextCheckCallbackData {
   public:
    TextCheckCallbackData(
        std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
        TextCheckCompleteCallback callback,
        base::string16 text);
    ~TextCheckCallbackData();

    // The URL loader used.
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader;

    // The callback function to be called when we receive a response from the
    // Spelling service and parse it.
    TextCheckCompleteCallback callback;

    // The text checked by the Spelling service.
    base::string16 text;

   private:
    DISALLOW_COPY_AND_ASSIGN(TextCheckCallbackData);
  };

  using SpellCheckLoaderList =
      std::list<std::unique_ptr<TextCheckCallbackData>>;

  void OnSimpleLoaderComplete(SpellCheckLoaderList::iterator it,
                              base::TimeTicks request_start,
                              std::unique_ptr<std::string> response_body);

  // List of loaders in use.
  SpellCheckLoaderList spellcheck_loaders_;

  // URL loader factory to use for fake network requests during testing.
  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELLING_SERVICE_CLIENT_H_
