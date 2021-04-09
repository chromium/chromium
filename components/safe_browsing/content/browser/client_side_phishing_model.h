// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/synchronization/lock.h"
#include "components/safe_browsing/content/browser/client_side_model_loader.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace safe_browsing {

struct ClientSidePhishingModelSingletonTrait;

// This holds the currently active client side phishing detection model.
//
// The data to populate it is fetched periodically from Google to get the most
// up-to-date model.
//
// This is thread safe. We assume it is updated at most every few hours.

class ClientSidePhishingModel {
 public:
  virtual ~ClientSidePhishingModel();

  static ClientSidePhishingModel* GetInstance();  // Singleton

  // Register a callback to be notified whenever the model changes.
  base::CallbackListSubscription RegisterCallback(
      base::RepeatingCallback<void()> callback);

  // Start loading the model with the given |url_loader_factory|
  void Start(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Cancel any pending model fetches.
  void Stop();

  // Returns whether we are currently actively fetching the model.
  bool IsEnabled() const;

  // Returns the model string, as a serialized protobuf.
  std::string GetModelStr() const;

  // Returns the model name.
  std::string GetModelName() const;

  // Returns the status of the most recent model fetch, or MODEL_NEVER_FETCHED
  // if we have done no fetches.
  ModelLoader::ClientModelStatus GetLastModelStatus() const;

  // Overrides the model string for use in tests.
  void SetModelStrForTesting(const std::string& model_str);

 private:
  static const int kInitialClientModelFetchDelayMs;

  ClientSidePhishingModel();

  // Callback when a new model proto has been fetched by |model_loader_|
  void ModelUpdatedCallback();

  // The list of callbacks to notify when a new model is ready. Protected by
  // lock_.
  base::RepeatingCallbackList<void()> callbacks_;

  // Fetches the ClientSideModel over the network. Protected by lock_.
  std::unique_ptr<ModelLoader> model_loader_;

  // Fake model string used in testing. Protected by lock_.
  std::string overridden_model_str_;

  mutable base::Lock lock_;

  friend struct ClientSidePhishingModelSingletonTrait;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_
