// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVER_URL_FETCHER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVER_URL_FETCHER_H_

#include "url/gurl.h"

namespace autofill_assistant {

class ServerUrlFetcher {
 public:
  ServerUrlFetcher(const GURL& server_url);
  virtual ~ServerUrlFetcher();

  // Returns the default server url. This is either the hard-coded constant or,
  // if applicable, the one provided via command-line argument.
  static GURL GetDefaultServerUrl();

  // Returns whether this instance points to the prod endpoint or not.
  virtual bool IsProdEndpoint() const;

  // Returns the endpoint to send the SupportsScript RPC to.
  virtual GURL GetSupportsScriptEndpoint() const;
  // Returns the endpoint to send the GetNextActions RPC to.
  virtual GURL GetNextActionsEndpoint() const;
  // Returns the endpoint to send the GetTriggerScripts RPC to.
  virtual GURL GetTriggerScriptsEndpoint() const;
  // Returns the endpoint to send the GetCapabilitiesByHashPrefix RPC to.
  virtual GURL GetCapabilitiesByHashEndpoint() const;
  // Returns the endpoint to send the GetUserData RPC to.
  virtual GURL GetUserDataEndpoint() const;

 private:
  GURL server_url_;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVER_URL_FETCHER_H_
