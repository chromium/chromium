# //components/input

The directory contains input processing code shared across browser and viz.
The requirement for this component came out of
[InputVizard](https://docs.google.com/document/d/1mcydbkgFCO_TT9NuFE962L8PLJWT2XOfXUAPO88VuKE/preview)
project, which proposes moving input handling off of browser main thread to viz
compositor thread. To execute InputVizard project, minimum required input
code is being moved in this component, such that browser and viz can depend on
it.

Please see
[cc/input/README.md](https://chromium.googlesource.com/chromium/src/+/HEAD/cc/input/README.md)
for information on input handling in compositor, and
[content/browser/renderer_host/input/README.md](https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/renderer_host/input/README.md)
for browser side input handling code.
