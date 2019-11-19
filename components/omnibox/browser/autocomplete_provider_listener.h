// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_LISTENER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_LISTENER_H_

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
  // NOTE: There's no parameter to tell the listener _which_ provider is
  // calling it.  Because the AutocompleteController (the typical listener)
  // doesn't cache the providers' individual matches locally, it has to get
  // them all again when this is called anyway, so such a parameter wouldn't
  // actually be useful.
  virtual void OnProviderUpdate(bool updated_matches) = 0;

 protected:
  virtual ~AutocompleteProviderListener() {}
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_LISTENER_H_
