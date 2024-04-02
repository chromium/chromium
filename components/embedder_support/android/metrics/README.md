# Embedder Support - Metrics

This submodule offers the metrics (mainly UMA logging) support required for
embedded web frameworks. In this context, "embedded web" means when web content
is rendered within other Android applications. The main client which consumes
this is Android WebView (//android_webview/), however other embedded web
libraries may also be able to use this and override functionality to suit their
needs.
