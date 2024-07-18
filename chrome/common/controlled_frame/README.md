# Controlled Frame API

*This directory contains common code used by the Controlled Frame API.*

The Controlled Frame API, available to certain isolated contexts such as
Isolated Web Apps (IWAs), allows an app to embed and manipulate arbitrary
content.

The API is built on top of the Chrome Apps &lt;webview&gt; tag and contains
the same API surface, except adapted for use in a non-extension setting.

Since it's built on top of &lt;webview&gt;, it makes use of the same underlying
components, including the GuestView component.

## More information

See the [Controlled Frame Explainer](https://github.com/WICG/controlled-frame)
and [Isolated Web Apps Explainer](https://github.com/WICG/isolated-web-apps/blob/main/README.md)
for more details.

Design doc (internal only): [go/controlled-frame-design](go/controlled-frame-design)
