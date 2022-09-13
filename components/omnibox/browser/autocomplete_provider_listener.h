// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_LISTENER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_LISTENER_H_

class AutocompleteProvider;

class AutocompleteProviderListener {
 public:
  // Called by a provider as a notification that something has changed.
  // |updated_matches| should be true iff the matches have changed in some
  // way (they may not have changed if, for example, the provider did an
  // asynchronous query to get more matches, came up with none, and is now
  // giving up).
  //
  // NOTE: Providers MUST only call this method while processing asynchronous
  // queries.  Do not call this for a synchronous query.
  //
  // NOTE: If a provider has finished, it should set done() to true BEFORE
  // calling this method.
  //
  // `provider` can be null when not called from a provider (e.g., on match
  // deletion).
  // TODO(manukh) Perhaps deleting matches shouldn't call `OnProviderUpdate()`.
  //   Not only is it semantically wrong, but it may also be unnecessary as
  //   `DeleteMatch()` already duplicates all the work `OnProviderUpdate()`
  //   does. Though `DeleteMatchElement()` doesn't.
  virtual void OnProviderUpdate(bool updated_matches,
                                const AutocompleteProvider* provider) = 0;

 protected:
  virtual ~AutocompleteProviderListener() = default;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_LISTENER_H_
