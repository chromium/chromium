Just-in-time installable payment handler that serves the payment method manifest
directly at the payment method identifier URL
https://a.com:<port>/nickpay.test/pay and has a relative URL in
"default_applications". This payment handler always can make payments and
responds to a payment request with {"status": "success"}.
