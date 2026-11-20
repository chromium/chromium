// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_RENDERER_KEEP_ALIVE_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_RENDERER_KEEP_ALIVE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/render_process_host_observer.h"

class Profile;
class TemplateURLService;

namespace content {
class RenderProcessHost;
}  // namespace content

// Lazily pins the most recently committed `chrome://new-tab-page/`
// `RenderProcessHost` per `Profile` via a pending-reuse refcount so subsequent
// NTP opens reuse the warm renderer process. Releases the pin on moderate+
// memory pressure, process exit/destruction, profile teardown, or when
// `chrome://new-tab-page/` stops being the profile's active NTP (policy,
// extension, or DSE override change), and lazily re-pins on the next eligible
// NTP load.
class NtpRendererKeepAlive : public base::SupportsUserData::Data,
                             public content::RenderProcessHostObserver,
                             public base::MemoryConsumer,
                             public ProfileObserver,
                             public TemplateURLServiceObserver {
 public:
  // Pins `rph` as `profile`'s warm NTP renderer when `kKeepNtpRendererAlive`
  // is enabled and `chrome://new-tab-page/` is the active NTP for `profile`.
  // No-op if `profile` or `rph` is null. Attaches `UserData` to `profile` and
  // holds a pending-reuse refcount on `rph` until released.
  static void MaybePin(Profile* profile, content::RenderProcessHost* rph);

  // `profile` must be non-null and outlive `this` (owned as `UserData` on
  // `profile` and cleared in `OnProfileWillBeDestroyed()`).
  explicit NtpRendererKeepAlive(Profile* profile);
  NtpRendererKeepAlive(const NtpRendererKeepAlive&) = delete;
  NtpRendererKeepAlive& operator=(const NtpRendererKeepAlive&) = delete;
  ~NtpRendererKeepAlive() override;

 private:
  void SetHost(content::RenderProcessHost* rph);
  void Release(bool decrement);
  void OnActiveNtpMightHaveChanged();

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // base::MemoryConsumer:
  void OnUpdateMemoryLimit() override;
  void OnReleaseMemory() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  raw_ptr<Profile> profile_;
  raw_ptr<content::RenderProcessHost> host_ = nullptr;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::ScopedObservation<content::RenderProcessHost,
                          content::RenderProcessHostObserver>
      host_observation_{this};
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};
  PrefChangeRegistrar pref_change_registrar_;
  base::MemoryConsumerRegistration memory_consumer_registration_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_RENDERER_KEEP_ALIVE_H_
