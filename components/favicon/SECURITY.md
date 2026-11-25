# Security Model for //components/favicon

`//components/favicon` (and `//components/favicon_base`) fetches, caches, and
persists website icons for display in browser UI. It runs in the browser
process, delegating untrusted resource parsing (such as image decoding and
manifest parsing) to sandboxed processes.

This document supplements Chromium's [Security FAQ](/docs/security/faq.md) and
[Security for Agents](/docs/security/security-for-agents.md).

## Threat Model

### Trust Assumptions

1. **Favicons Are Untrusted UI Metadata:**
   Any website can serve arbitrary imagery as its icon. Favicons are decorative
   and are not an identity or security indicator; origin identity is conveyed by
   browser-controlled UI (such as the address bar).
2. **Web Content & Renderers Are Untrusted:**
   Icon URLs and decoded bitmaps originate from untrusted web content and
   sandboxed processes. Page URLs and origins used for access checks must come
   from browser-trusted navigation state.
3. **Internal Sources & Local Storage Are Trusted:**
   Icon data from Chrome Sync, local importers, Google's favicon server (over
   HTTPS), and local profile storage are trusted with respect to icon
   attribution, though malformed data must still be handled without memory
   corruption.

### In-Scope Vulnerabilities

* **Cross-Origin History or State Leaks (XS-Leaks):** Allowing a web origin to
  infer whether the user visited another origin, or which icon URLs another
  origin uses, via web-observable cache behavior such as network fetch
  suppression or timing (except as noted below for redirects).
* **Off-the-Record (Incognito) Leaks:** Persisting favicon state from an
  off-the-record session, or allowing web content in an off-the-record session
  to infer state from the regular profile's cache.
* **Memory Corruption or Privilege Escalation:** Browser-process memory safety
  bugs, SQL injection, or unauthorized script/privilege execution reachable from
  web content or a compromised renderer.

### Out of Scope / Not a Security Boundary

* **Visual Icon Spoofing or Misattribution:** Causing browser UI to display a
  wrong, stale, or attacker-chosen icon (even for another origin), provided
  browser-controlled origin indicators remain accurate. For storage and UX
  efficiency, icon storage and fallback lookups are not strictly isolated per
  origin.
* **Arbitrary Icons for the Current Page:** A page or compromised renderer
  supplying arbitrary icon URLs or decoded bitmaps for its own navigation, as
  long as it does not leak cross-origin state or corrupt the browser process.
* **Redirect-Chain Icon Association:** Intermediate URLs in a navigation
  redirect chain are intentionally associated with the destination page's icon
  (e.g., for redirecting bookmarks). While this must not grant origin-wide cache
  access to the redirecting origin, direct lookups for the redirecting URL
  itself returning that icon are working as intended.
* **Trusted Sources & Direct File Access:** Icon modifications arriving via
  Chrome Sync, local importers, or direct local access to the profile directory.
