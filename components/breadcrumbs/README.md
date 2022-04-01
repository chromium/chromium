Breadcrumbs appends a short history of events to crash reports. These help to
diagnose the cause of the attached crash by providing context around when it
happened.

Breadcrumb events include:
* user-triggered actions listed in tools/metrics/actions.xml
* per-tab actions, e.g., when navigation starts/finishes
* per-browser actions, e.g., number of tabs opened/closed/moved
* memory pressure
* startup/shutdown

Breadcrumbs are not uploaded or stored on disk unless the user has consented to
metrics reporting.
