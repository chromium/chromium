---
name: chrome/map-deep-scan-access-point-for-file-attachments
rubric: correctness
severity: should_fix
---

**Rule.** When categorizing enterprise content analysis events by connector
action (such as distinguishing file attachments from file transfers), do not
branch solely on `enterprise_connectors::DeepScanAccessPoint::UPLOAD`.
Explicitly handle `DRAG_AND_DROP`, `PASTE`, and `ACTOR` alongside `UPLOAD` (or
in `files_request_handler_base.cc`, map the access point via the file-local
`AccessPointToEnterpriseConnector()` / `AccessPointToUmaHistogramPrefix()`
helpers) so all file attachment triggers are included.

**Why.** In Enterprise Connectors, the file attachment connector
(`FILE_ATTACHED`) encompasses files selected via the file picker (`UPLOAD`),
dragged and dropped into the browser (`DRAG_AND_DROP`), pasted from clipboard
(`PASTE`), or initiated programmatically (`ACTOR`). Branching solely on
`DeepScanAccessPoint::UPLOAD` inadvertently omits drag-and-drop, clipboard
paste, and actor events, leading to undercounted metrics, missed scanning, or
incorrect behavior.

## Examples

Bad:

```cpp
// Flawed: Only checks UPLOAD, missing drag-and-drop, clipboard paste, and actor
// file attachments.
switch (access_point()) {
  case enterprise_connectors::DeepScanAccessPoint::UPLOAD:
    base::UmaHistogramCustomCounts("Enterprise.OnFileAttach.FileCount",
                                   file_count, 1, 1000, 100);
    break;
  case enterprise_connectors::DeepScanAccessPoint::FILE_TRANSFER:
    base::UmaHistogramCustomCounts("Enterprise.OnFileTransfer.FileCount",
                                   file_count, 1, 1000, 100);
    break;
  default:
    break;
}
```

Good:

```cpp
// Correct: Explicitly groups all file attachment access points (or uses
// AccessPointToUmaHistogramPrefix() in files_request_handler_base.cc).
switch (access_point()) {
  case enterprise_connectors::DeepScanAccessPoint::UPLOAD:
  case enterprise_connectors::DeepScanAccessPoint::DRAG_AND_DROP:
  case enterprise_connectors::DeepScanAccessPoint::PASTE:
  case enterprise_connectors::DeepScanAccessPoint::ACTOR:
    base::UmaHistogramCustomCounts("Enterprise.OnFileAttach.FileCount",
                                   file_count, 1, 1000, 100);
    break;
  case enterprise_connectors::DeepScanAccessPoint::FILE_TRANSFER:
    base::UmaHistogramCustomCounts("Enterprise.OnFileTransfer.FileCount",
                                   file_count, 1, 1000, 100);
    break;
  default:
    break;
}
```
