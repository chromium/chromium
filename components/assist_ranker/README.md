# Assist Ranker

[TOC]

## Overview

The generic machine learning inference infrastructure and predictors formerly
hosted in this component have been removed.

The remaining utilities in this component support:

* **Example Preprocessing**: Feature preprocessing and vectorization utilities
  (`ExamplePreprocessor`, `RankerExample`) used by ChromeOS Smart Dim
  (`//chrome/browser/ash/power/ml/smart_dim`).
* **Model Loading**: Model downloading, caching, and validation infrastructure
  (`RankerModelLoader`, `RankerModel`) used by Translate Ranker
  (`//components/translate/core/browser`).