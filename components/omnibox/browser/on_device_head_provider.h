// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/on_device_head_model.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"

// TODO(crbug.com/40241602): clean up this build flag guard later if possible.
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/omnibox/browser/on_device_tail_model_executor.h"
#endif

class AutocompleteProviderListener;

// An asynchronous autocomplete provider which receives input string and tries
// to find the matches in an on device head model. This provider is designed to
// help users get suggestions when they are in poor network.
// By default, all matches provided by this provider will have a relevance no
// greater than 99, such that its matches will not show before any other
// providers; However the relevance can be changed to any arbitrary value by
// Finch when the input is not classified as a URL.
// TODO(crbug.com/40241602): rename this provider to "OnDeviceProvider" since it
// will serve both head and tail suggestions.
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
  OnDeviceHeadProvider(const OnDeviceHeadProvider&) = delete;
  OnDeviceHeadProvider& operator=(const OnDeviceHeadProvider&) = delete;

  bool IsOnDeviceHeadProviderAllowed(const AutocompleteInput& input);

  // Helper functions used for asynchronous search to the on device models.
  // The Autocomplete input and output from the model will be passed from
  // DoSearch to AllSearchDone via the OnDeviceHeadProviderParams object.
  // DoSearch: searches the on device models and returns the tops suggestions
  // matches the given AutocompleteInput.
  void DoSearch(std::unique_ptr<OnDeviceHeadProviderParams> params);
  // AllSearchDone: called after all searches are completed, fills |matches_|
  // with the suggestions fetched by DoSearch and then calls NotifyListeners.
  void AllSearchDone(std::unique_ptr<OnDeviceHeadProviderParams> params);
  // Helper function to be called when searches to the head model is done.
  void HeadModelSearchDone(std::unique_ptr<OnDeviceHeadProviderParams> params);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Helper function to be called when searches to the tail model is done.
  void TailModelSearchDone(
      std::unique_ptr<OnDeviceHeadProviderParams> params,
      std::vector<OnDeviceTailModelExecutor::Prediction> predictions);
#endif

  // Helper functions to read head model filename from the static
  // OnDeviceModelUpdateListener instance.
  std::string GetOnDeviceHeadModelFilename() const;

  // Fetches suggestions matching the params from the given on device head
  // model.
  static std::unique_ptr<OnDeviceHeadProviderParams>
  GetSuggestionsFromHeadModel(
      const std::string& model_filename,
      const size_t provider_max_matches,
      std::unique_ptr<OnDeviceHeadProviderParams> params);

  // Determines whether should fetch tail suggestions.
  static bool ShouldFetchTailSuggestions(
      const OnDeviceHeadProviderParams& params);

  raw_ptr<AutocompleteProviderClient> client_;

  // The task runner dedicated for on device model operations which is added to
  // offload expensive operations out of the UI sequence.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  // Sequence checker that ensure autocomplete request handling will only happen
  // on main thread.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // The request id used to trace current request to the on device models.
  // The id will be increased whenever a new request is received from the
  // AutocompleteController.
  size_t on_device_search_request_id_;

  base::WeakPtrFactory<OnDeviceHeadProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_PROVIDER_H_
