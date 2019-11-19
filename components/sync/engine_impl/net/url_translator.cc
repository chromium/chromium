// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/net/url_translator.h"

#include "build/branding_buildflags.h"
#include "net/base/escape.h"

using std::string;

namespace syncer {

namespace {
// Parameters that the server understands. (here, a-Z)
const char kParameterClient[] = "client";
const char kParameterClientID[] = "client_id";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kClientName[] = "Google Chrome";
#else
const char kClientName[] = "Chromium";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace

// This method appends the query string to the sync server path.
string MakeSyncServerPath(const string& path, const string& query_string) {
  string result = path;
  result.append("?");
  result.append(query_string);
  return result;
}

string MakeSyncQueryString(const string& client_id) {
  string query;
  query += kParameterClient;
  query += "=" + net::EscapeUrlEncodedData(kClientName, true);
  query += "&";
  query += kParameterClientID;
  query += "=" + net::EscapeUrlEncodedData(client_id, true);
  return query;
}

}  // namespace syncer
