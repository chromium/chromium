# Desktop Web Applications UI (`chrome/browser/ui/web_applications`)

**Parent:** [WebApps Central Hub](/components/webapps/AGENTS.md)

Desktop PWA UI controllers bridging the backend `WebAppProvider` with Chrome
browser windows, tabs, omnibox install actions, navigation capturing, and Mac
App Shims.

## Canonical Docs

- [README.md](README.md)

## Invariants

- **`AppBrowserController` vs. `BrowserWindowInterface::Type`:**
  `AppBrowserController::IsWebApp(browser)` (and `From(browser)`) returns `true`
  for `TYPE_APP`, `TYPE_APP_POPUP`, and app-opened `TYPE_FLOATING` (Document
  Picture-in-Picture) windows, while `IsWebApp(browser)` returns `false` for
  `TYPE_NORMAL`, `TYPE_POPUP`, normal-tab `TYPE_FLOATING`, and `TYPE_DEVTOOLS`.
  Never assume `IsWebApp(b)` implies `TYPE_APP` or `!IsWebApp(b)` implies
  `TYPE_NORMAL`; callers that specifically require a `TYPE_APP` or `TYPE_NORMAL`
  window must check `browser->GetType()`. See the
  [Window State & Controller Matrix](README.md#window-state-controller-matrix)
  for details.
