The page load metrics subsystem captures and records UMA metrics related to
page loading. This includes page timing metrics, such as time to first
paint, as well as page interaction metrics, such as number of page loads
aborted before first paint.

The page load metrics subsystem is shared by Chrome and WebView, but not
supported on iOS, as it requires hooks into Blink for page timing metrics that
are unavailable on iOS.

This component has the following structure and these directories follow the rule
that //content follows. If your new file needs to depend on something that
//content disallows, you need to consider to place it outside
//components/page_load_metrics/, e.g. //chrome/browser/page_load_metrics/ for
Chrome, or //components/page_load_metrics/google/ if it depends on Google specific
properties and is used from multiple embedders, e.g. Chrome and WebView.
- //components/page_load_metrics/browser/: browser process code
- //components/page_load_metrics/common/: code shared by browser and renderer, such
as IPC messages
- //components/page_load_metrics/renderer/: renderer process code

For additional information on this subsystem, see
https://docs.google.com/document/d/1HJsJ5W2H_3qRdqPAUgAEo10AF8gXPTXZLUET4X4_sII/edit
