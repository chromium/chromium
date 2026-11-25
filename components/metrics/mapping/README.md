# Metrics Name Mapping

This directory contains generic logic and configuration schemas for renaming
and mapping metrics emitted by specific subprocesses before they are merged
into the browser's global metrics logs.

See [the design doc][1] and [the configuration schema][2].

[1]: https://docs.google.com/document/d/1sYiMun4Sif9BpoWpQv6Ks6SFjPri58qFgRNwSP7RrZU/edit?resourcekey=0-1o82VfCU2bfN0pkXdnF8ag
[2]: https://docs.google.com/document/d/1SudIQmC_nrfyDzqXcDDDZP6ZN596FFm56rpFdoJnHFs/edit?resourcekey=0-JZy13P2VyfvbrTSNzWD27w&tab=t.0

## Configuration

The generic mapping strategy utilizes a `MetricsNameMapper` class initialized
with a Base-64 encoded protobuf string representing the
`MetricsNameMappingConfiguration`.

This configuration defines a `rules` array in `metrics_name_mapping.proto`
which dictates whether a strictly matching histogram string should be dropped or
mapped to a `new_metric_name`.

An embedder can also register built-in rules and allowed metric name prefixes
with `MetricsNameMapper::SetDefaults()`. `MetricsNameMapper` uses the first
match in this order: built-in rules, server `rules`, allowed prefixes (kept
unchanged), then drop. Built-in rules come first so that a server rule, such as
one that keeps a metric under its own name, cannot undo a built-in rename.
Disabling `WebiumMetricsMapping` turns off both. Like server rules, the defaults
apply only to histograms that `MetricsNameMapper` evaluates, all of which come
from top-chrome WebUI renderers (`IsWebiumRenderer()` in
`SubprocessMetricsProvider`, `is_webium_renderer` in `HistogramSynchronizer`).
An empty or invalid server configuration is ignored, so the defaults still
apply.

The `MetricsNameMapper` evaluator is called dynamically by the
`SubprocessMetricsProvider` when parsing and merging metric allocation streams
from child processes.
