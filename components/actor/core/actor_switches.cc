// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/actor_switches.h"

namespace actor::switches {

// Bypasses several of the actor's safety checks, such as requiring SafeBrowsing
// to be enabled and the blocklist.
// Note that some checks, like requiring https, are not affected by this.
const char kDisableActorSafetyChecks[] = "disable-actor-safety-checks";

// Bypasses the selection of the data to be filled by the
// `AttemptFormFillingTool` and just picks the first suggestion for each form
// section. This is only intended for testing.
const char kAttemptFormFillingToolSkipsUI[] =
    "attempt-form-filling-tool-skips-ui";

// Forces logging of the actor aggregated journal events in VLOG(1). Useful on
// Android as VLOGs are removed on official builds.
const char kEnableActorJournalVLog[] = "enable-actor-journal-vlog";

// File or directory path where actor traces should be recorded.
const char kActorTracePath[] = "actor-trace-path";

// Specifies a comma-separated list of SchemefulSite strings that should be
// treated as sensitive sites by the actor execution engine (bypassing the
// optimization guide component updater check). Useful for testing.
// Note that these entries are site-scoped (scheme and eTLD+1), not
// origin-scoped, so specifying a site will match all subdomains and ports on
// that site.
// Examples:
//   --actor-sensitive-sites="https://example.com"
//   --actor-sensitive-sites="https://bank.com,https://example.org"
const char kActorSensitiveSites[] = "actor-sensitive-sites";

}  // namespace actor::switches
