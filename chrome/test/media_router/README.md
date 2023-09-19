<!--
Copyright 2017 The Chromium Authors
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
-->

# Media Router Integration and E2E Browser Tests

This directory contains the integration and end-to-end browser tests for Media
Router.  The Media Router uses various Media Route Providers to connect to
different types of receivers (sinks).

## Tests

* `MediaRouterIntegrationBrowserTest`: Tests that Media Router behaves as
specified by the Presentation API, and that its dialog is shown as expected
using the test provider `TestMediaRouteProvider`. Test cases that specifically
test the functionalities of the Media Router dialog are in
`media_router_integration_ui_browsertest.cc`.

* `MediaRouterE2EBrowserTest`: Tests Chromecast-specific functionality of Media
Router using the Cast Media Route Provider.  Requires an actual Chromecast
device.

* `MediaRouterIntegrationOneUABrowserTest`: Tests that the Presentation API can
be used to start presentations using offscreen tabs, and that basic Presentation
API usage with offscreen tabs is working.

* `MediaRouterIntegrationOneUANoReceiverBrowserTest`: Tests Presentation API
behavior when there is no compatible presentation receiver for the requested
URL.
