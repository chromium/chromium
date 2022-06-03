# chromecast/browser/mojom

These mojo interfaces act as a frontend for the "Cast Browser", a barebones
content embedder targeting embedded devices such as Chromecast. All clients for
these interfaces are fully trusted; the main client is the "Cast Service", which
hosts all of supporting logic for the Cast protocol. We never allow an untrusted
process to use these interfaces (e.g. Chromium render processes).

The main interfaces in this folder are:

* CastWebService: Main entry point for browser initialization,
  creating/displaying pages, and mutating browser state.
* CastWebContents: Simplified wrapper for WebContents. Exposes a subset of
  content::WebContentsObserver events so that the client can respond to
  spontaneous page state changes, such as a page closing itself or crashing.
* CastContentWindow: Windowing logic for CastWebContents. This is backed by
  different window primitives depending on the platform, such as Android
  Activities/Fragments, aura::Window, or other system window embedder.
