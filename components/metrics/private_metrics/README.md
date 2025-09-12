# Overview
**Private Metrics** is a comprehensive system that encompasses both the **Deidentified Web Analytics (DWA)** and the **Domain Keyed Metrics (DKM)**. The purpose of Private Metrics is to offer anonymization solutions primarily for Chrome Metrics. It is designed for sensitive metrics that require privacy guarantees beyond what existing systems UMA/UKM provide. Private Metrics does this by leveraging the use of trusted execution environments (TEEs) and employing anonymization workloads like k-anonymity and trusted proxy.

The Private Metrics system is composed of the following components:

* **`DwaBuilder` / `DkmBuilder`**: The builder interface exposes an interface metrics owners and developers can use to build metric entries.
* **`DwaEntryBuilder` / `DkmEntryBuilder`**: The builder interface exposes an interface metrics owners and developers can use to build metric entries where it is not feasible to use `DwaBuilder` / `DkmBuilder`. More information can be found in "components/metrics/dwa/dwa_entry_builder_base.h" and "//components/metrics/private_metrics/dkm_entry_builder_base.h", respectively.
* **`DwaRecorder` / `DkmRecorder`**: The recorder accepts entries from DwaEntryBuilder/DkmEntryBuilder respectively and records the metrics in memory.
* **`PrivateMetricsService`**: The private metrics service is responsible for generating and encrypting reports from in-memory events collected in DwaRecorder and DkmRecorder, storing reports to disk, and scheduling reports for upload.
* **`PrivateMetricsReportingService`**: This service is responsible for reporting reports collected in PrivateMetricsService to Google's servers.

For more information, see: go/private-metrics (internal only).
