This directory provides an evolving abstraction of classes like `Browser`
(`chrome/browser/ui/browser.h`), `BrowserWindowInterface`, `BrowserList`,
and `TabStripModel` for use by ChromeOS feature code.

The abstraction consists of two (C++) interfaces, `BrowserDelegate` &
`BrowserController`:
* `BrowserDelegate`: An abstract version of the existing `Browser` and
  `BrowserWindowInterface` class.
* `BrowserController`: A supervising entity that primarily creates or finds
  `BrowserDelegate` objects.

Development is incremental; in the first phase we prioritize making these
classes the bottleneck for browser access over them offering a clean API.

The Concrete implementations (`BrowserDelegateImpl` and `BrowserControllerImpl`)
reside in `//chrome/browser/ash/browser_delegate/`.
