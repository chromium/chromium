---
name: webapps-app-service-review
description: >-
  Reviews WebAppPublisherHelper in chrome/browser/web_applications/app_service/
  for synchronous PreferredAppsList changes inside
  PreferredAppsListHandle::Observer callbacks. Use when a CL changes
  WebAppPublisherHelper, its preferred apps observer callbacks, or the link
  capturing preference migration.
---

# Web Applications App Service Review

## `WEBAPPS_NO_SYNC_PREFERRED_APPS_CHANGE_IN_OBSERVER`: Do not change `PreferredAppsList` synchronously inside a `PreferredAppsListHandle::Observer` callback

- **Severity**: ERROR
- **Why**: `PreferredAppsList` notifies observers while it iterates over a
  `base::ObserverList`. A synchronous preferred apps change inside the callback
  re-enters the `ObserverList` notification and crashes.
- **Look for**: Synchronous calls to
  `AppServiceProxy::SetSupportedLinksPreference()`,
  `AppServiceProxy::RemoveSupportedLinksPreference()`, or helpers that call them
  (for example `MaybeMigrateLinkCapturingPreferences()`) inside
  `apps::PreferredAppsListHandle::Observer` callbacks in
  `WebAppPublisherHelper`, such as `OnPreferredAppsListInitialized()`.
- **Do not flag**: Callbacks that post the change with
  `base::SequencedTaskRunner::GetCurrentDefault()->PostTask()` and
  `weak_ptr_factory_.GetWeakPtr()`. Unit tests and browser tests in
  `app_service/`. `PreferredAppsList` modifications that occur in a callback
  that was posted as a task, or changes to the list in tests.
