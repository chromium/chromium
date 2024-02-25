The app_restore component contains code necessary for collecting app launching
information, app window information, and writing to the data storage. It
provides:

* The interfaces for chrome/browser/ash/app_restore to read the storage
to get app launching information.
* The interfaces for the Window Management component (ash/wm) to collect and
save the app windows information.
* The interfaces for AppService (chrome/browser/apps/app_service) to save the
app launching information.
* The interfaces for components/exo to set the window restoration properties.
