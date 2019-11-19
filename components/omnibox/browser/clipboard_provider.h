// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_PROVIDER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/history_url_provider.h"

class AutocompleteProviderClient;
class ClipboardRecentContent;
class HistoryURLProvider;

// Autocomplete provider offering content based on the clipboard's content.
class ClipboardProvider : public AutocompleteProvider {
 public:
  ClipboardProvider(AutocompleteProviderClient* client,
                    AutocompleteProviderListener* listener,
                    HistoryURLProvider* history_url_provider,
                    ClipboardRecentContent* clipboard_content);

  // AutocompleteProvider implementation.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;
  void ResetSession() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ClipboardProviderTest, MatchesImage);

  ~ClipboardProvider() override;

  // Handle the match created from one of the match creation methods and do
  // extra tracking and match adding.
  void AddCreatedMatchWithTracking(
      const AutocompleteInput& input,
      const AutocompleteMatch& match,
      const base::TimeDelta clipboard_contents_age);

  // If there is a url copied to the clipboard, use it to create a match.
  base::Optional<AutocompleteMatch> CreateURLMatch(
      const AutocompleteInput& input);
  // If there is text copied to the clipboard, use it to create a match.
  base::Optional<AutocompleteMatch> CreateTextMatch(
      const AutocompleteInput& input);
  // If there is an image copied to the clipboard, use it to create a match.
  // The image match is asynchronous (because constructing the image post data
  // takes time), so instead of returning an optional match like the other
  // Create functions, it returns a boolean indicating whether there will be a
  // match.
  bool CreateImageMatch(const AutocompleteInput& input);

  // Resize and encode the image data into bytes. This can take some time if the
  // image is large, so this should happen on a background thread.
  static scoped_refptr<base::RefCountedMemory> EncodeClipboardImage(
      gfx::Image image);
  // Construct the actual image match once the image has been encoded into
  // bytes. This should be called back on the main thread.
  void ConstructImageMatchCallback(
      const AutocompleteInput& input,
      TemplateURLService* url_service,
      base::TimeDelta clipboard_contents_age,
      scoped_refptr<base::RefCountedMemory> image_bytes);

  AutocompleteProviderClient* client_;
  AutocompleteProviderListener* listener_;
  ClipboardRecentContent* clipboard_content_;

  // Used for efficiency when creating the verbatim match.  Can be NULL.
  HistoryURLProvider* history_url_provider_;

  // The current URL suggested and the number of times it has been offered.
  // Used for recording metrics.
  GURL current_url_suggested_;
  size_t current_url_suggested_times_;

  // Whether a field trial has triggered for this query and this session,
  // respectively. Works similarly to BaseSearchProvider, though this class does
  // not inherit from it.
  bool field_trial_triggered_;
  bool field_trial_triggered_in_session_;

  // Used to cancel image construction callbacks if autocomplete Stop() is
  // called.
  base::WeakPtrFactory<ClipboardProvider> callback_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClipboardProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_PROVIDER_H_
