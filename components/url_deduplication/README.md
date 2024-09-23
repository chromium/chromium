# URL Deduplication Component

Component that facilitates deduplication of similar urls based on a deduplication strategy for
use in applications such as the most relevant tab resumption module.

## URL Deduplication services

There is a common interface to fulfill data fetching requirements:

* [DeduplicationStrategy](/components/url_deduplication/deduplication_strategy.h)
Common interface that describes the strategy that will be used for deduplication.


* [URLStripHandler](/components/url_deduplication/url_strip_handler.h)
Interface that holds the methods that will be used in the deduplication helper.
