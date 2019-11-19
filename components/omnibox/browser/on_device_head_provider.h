// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/on_device_head_serving.h"
#include "components/omnibox/browser/on_device_model_update_listener.h"

class AutocompleteProviderListener;

// An asynchronous autocomplete provider which receives input string and tries
// to find the matches in an on device head model. This provider is designed to
// help users get suggestions when they are in poor network.
// By default, all matches provided by this provider will have a relevance no
// greater than 99, such that its matches will not show before any other
// providers; However the relevance can be changed to any arbitrary value by
// Finch when the input is not classified as a URL.
class OnDeviceHeadProvider : public AutocompleteProvider {
 public:
  static OnDeviceHeadProvider* Create(AutocompleteProviderClient* client,
                                      AutocompleteProviderListener* listener);

  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

  AutocompleteProviderClient* client() { return client_; }

 private:
  friend class OnDeviceHeadProviderTest;

  // A useful data structure to store Autocomplete input and suggestions fetched
  // from the on device head model for a search request to the model.
  struct OnDeviceHeadProviderParams;

  OnDeviceHeadProvider(AutocompleteProviderClient* client,
                       AutocompleteProviderListener* listener);
  ~OnDeviceHeadProvider() override;

  bool IsOnDeviceHeadProviderAllowed(const AutocompleteInput& input,
                                     const std::string& incognito_serve_mode);

  // Helper functions used for asynchronous search to the on device head model.
  // The Autocomplete input and output from the model will be passed from
  // DoSearch to SearchDone via the OnDeviceHeadProviderParams object.
  // DoSearch: searches the on device model and returns the tops suggestions
  // matches the given AutocompleteInput.
  void DoSearch(std::unique_ptr<OnDeviceHeadProviderParams> params);
  // SearchDone: called after DoSearch, fills |matches_| with the suggestions
  // fetches by DoSearch and then calls OnProviderUpdate.
  void SearchDone(std::unique_ptr<OnDeviceHeadProviderParams> params);

  // Used by OnDeviceModelUpdateListener to notify this provider when new model
  // is available.
  void OnModelUpdate(const std::string& new_model_filename);

  // Resets |serving_| if new model is available and cleans up the old model if
  // it exists.
  void ResetServingInstanceFromNewModel(const std::string& new_model_filename);

  AutocompleteProviderClient* client_;
  AutocompleteProviderListener* listener_;

  // The instance which does the search in the head model and returns top
  // suggestions matching the Autocomplete input.
  std::unique_ptr<OnDeviceHeadServing> serving_;

  // The task runner instance where asynchronous operations using |serving_|
  // will be run. Note that SequencedTaskRunner guarantees that operations will
  // be executed in sequence so we don't need to apply a lock to |serving_|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The request id used to trace current request to the on device head model.
  // The id will be increased whenever a new request is received from the
  // AutocompleteController.
  size_t on_device_search_request_id_;

  // The filename for the on device model currently being used.
  std::string current_model_filename_;

  // Owns the subscription after adding the model update callback to the
  // listener such that the callback can be removed automatically from the
  // listener on provider's deconstruction.
  std::unique_ptr<OnDeviceModelUpdateListener::UpdateSubscription>
      model_update_subscription_;

  base::WeakPtrFactory<OnDeviceHeadProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OnDeviceHeadProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_
