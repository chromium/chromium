# ChromeOS Printing

`chromeos/printing/` contains ChromeOS printing code that doesn't depend on
`//chrome`. It primarily contains code dealing with local printing via the
Common Unix Printing System (CUPS).

## Other Related Directories

(Paths are given from the git root):

* `chrome/browser/ui/webui/ash/settings/pages/printing/` - ChromeOS printing
  settings dialog backend support
* `chrome/browser/resources/ash/settings/os_printing_page/` - Front end printer
  settings code.
* `chrome/browser/printing/` - Cloud print support, and common print dialog
  support.
* `chrome/browser/ash/printing/` - ChromeOS printing code that runs in
  ash-chrome.

## Printing Docs

* [Cups Printer Management](cups_printer_management.md) - Overview of how CUPS
  printers are managed in ChromeOS.
* [Cups Printers Settings Page](cups_printers_settings_page.md) - Contains
  information about the various dialogs, pages, and various messages/listeners
  in the UI.
