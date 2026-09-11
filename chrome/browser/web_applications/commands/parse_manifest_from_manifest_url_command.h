// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_PARSE_MANIFEST_FROM_MANIFEST_URL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_PARSE_MANIFEST_FROM_MANIFEST_URL_COMMAND_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/model/parse_manifest_result.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

class ParseManifestFromStringJob;

// Used for manifest-first install flows to parse `manifest_contents`, the raw
// JSON previously fetched from `manifest_url`. Parses in the context of
// about:blank and resolves relative URLs using `manifest_url`.
//
// Returns the parsed manifest or a structured failure reason.
//
// NOTE!! This command may be scheduled from off-the-record profiles via
// GetOriginalProfile(). It performs read-only parsing and never modifies
// the web app registrar.
class ParseManifestFromManifestUrlCommand
    : public WebAppCommand<SharedWebContentsLock, ParseManifestResult> {
 public:
  using ParseCallback = base::OnceCallback<void(ParseManifestResult)>;

  // `manifest_url`: The URL the manifest was fetched from. Used as both
  //   document_url and manifest_url for ManifestParser, enabling correct
  //   relative URL resolution within the manifest.
  // `manifest_contents`: The raw JSON string fetched from manifest_url.
  // `callback`: Called with the parsed manifest or failure reason.
  ParseManifestFromManifestUrlCommand(GURL manifest_url,
                                      std::string manifest_contents,
                                      ParseCallback callback);

  ~ParseManifestFromManifestUrlCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

 private:
  void OnJobComplete(ParseManifestResult parse_result);

  GURL manifest_url_;
  std::string manifest_contents_;

  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;
  std::unique_ptr<ParseManifestFromStringJob> parse_job_;

  base::WeakPtrFactory<ParseManifestFromManifestUrlCommand> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_PARSE_MANIFEST_FROM_MANIFEST_URL_COMMAND_H_
