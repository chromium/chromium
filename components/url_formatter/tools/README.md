# IDN Tools

This directory contains tools to help with changes related to the handling of Internationalized Domain Names (IDN) related changes.

### format_url

This binary takes a list of domain names, passes them through url_formatter and prints the result. This is useful when measuring the impact of spoof checks in `components/url_formatter` over a large list of domains.

